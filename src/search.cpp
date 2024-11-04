#include <iostream>

#include "eval.hpp"
#include "hash.hpp"
#include "search.hpp"
#include "thread.hpp"
#include "uci.hpp"

namespace Maestro {

struct Config;

void SearchWorker::clear() {
  killerTable.fill(Move::none());
  counterMoveTable.fill(Move::none());
  historyTable.fill(0);
  captureHistoryTable.fill(0);
  continuationTable.fill(StatsEntry<ContinuationHistory, NOT_USED>{});
}

void SearchWorker::getPV(SearchWorker &best, Depth depth) const {

  auto &rootMoves = best.rootMoves;
  auto &pos = best.rootPos;

  const U64 nodes = threads.nodesSearched();

  for (size_t i = 0; i < sharedState.config.multiPV; ++i) {
    bool updated = rootMoves[i].score != -VAL_INFINITE;

    if (depth == 1 && !updated && i > 0)
      continue;

    Depth d = updated ? depth : std::max(1, depth - 1);
    Value v = updated ? rootMoves[i].score : rootMoves[i].prevScore;

    if (v == -VAL_INFINITE)
      v = VAL_ZERO;

    bool isExact = i != pvIdx || !updated;

    std::string pv;

    for (Move m : rootMoves[i].pv)
      pv += move2Str(m) + " ";

    if (!pv.empty())
      pv.pop_back();

    PrintInfo info;

    info.depth = d;
    info.score = v;
    info.selDepth = rootMoves[i].selDepth;
    info.multiPVIdx = i + 1;
    info.timeMs = tm.elapsed() + 1;
    info.nodes = nodes;
    info.nps = nodes * 1000 / info.timeMs;
    info.pv = pv;
    info.hashFull = tt.hashFull();

    UCI::uciReport(info);
  }
}

void SearchWorker::updatePV(Move *pv, Move best, const Move *childPV) const {

  for (*pv++ = best; childPV && *childPV != Move::none();)
    *pv++ = *childPV++;

  *pv = Move::none();
}

void SearchWorker::startSearch() {

  if (!isMainThread()) {
    iterativeDeepening();
    return;
  }

  tm.init(limits, rootPos.getSideToMove(), rootPos.getPliesFromStart(),
          sharedState.config);

  tt.newSearch();

  if (rootMoves.empty()) {
    rootMoves.emplace_back(Move::none());
  } else {
    threads.startSearch();
    iterativeDeepening();
  }

  threads.stop = true;

  threads.waitForThreads();

  SearchWorker *best = this;

  if (sharedState.config.multiPV == 1 and !limits.depth and !limits.mate and
      rootMoves[0].pv[0] != Move::none())
    best = threads.getBestThread()->worker.get();

  bestPreviousAvgScore = best->rootMoves[0].averageScore;
  bestPreviousScore = best->rootMoves[0].score;

  if (best != this) {
    getPV(*best, best->completedDepth);
  }

  auto bestMove = move2Str(best->rootMoves[0].pv[0]);
  std::cout << "bestmove " << bestMove << std::endl;
}

bool SearchWorker::checkTM(Depth &lastBestMoveDepth, int &pvStability,
                           int &bestValue) const {
  if (completedDepth <= 4)
    return false;

  bool stable = lastBestMoveDepth + 3 <= completedDepth;

  pvStability = stable ? std::min(10, pvStability + 1) : 0;

  // Scale time between 80% and 120% , based on stability of best moves
  const double pvFactor = 1.2 - 0.04 * pvStability;

  // Scale time between 80% and 120% , based on score fluctuation
  const double scoreFluctuation = abs(bestPreviousAvgScore - bestValue);
  const double scoreFactor = 1.20 - 0.04 * std::min(scoreFluctuation, 10.00);

  // Scale time between 50% and 240%, based on what % of nodes are used for
  // searching the best move
  const U64 nodesUsedForBestMove = rootMoves[0].effort;
  const double nodesBestPercent =
      nodesUsedForBestMove / threads.nodesSearched();
  const double nodesFactor = std::max(0.5, 2 * nodesBestPercent + 0.4);

  double totalTime = tm.optimum() * pvFactor * scoreFactor * nodesFactor;

  auto elapsed = tm.elapsed();

  return elapsed >= totalTime;
}

void SearchWorker::checkTime() const {
  if (isMainThread() and completedDepth >= 1 and
      ((limits.useTimeManagement() and tm.elapsed() >= tm.maximum()) or
       (limits.movetime and tm.elapsed() >= limits.movetime)))
    threads.stop = threads.abortedSearch = true;
}

void SearchWorker::iterativeDeepening() {

  Move pv[MAX_PLY + 1];

  Depth lastBestMoveDepth = 0;
  Value lastBestMoveScore = -VAL_INFINITE;
  auto lastBestPV = std::vector<Move>{Move::none()};

  Value alpha, beta;
  Value bestValue = -VAL_INFINITE;
  Colour us = rootPos.getSideToMove();
  int pvStability = 0;
  int delta = 0;

  // // Allocate search stack
  // // (ss - 3) is accessed from (ss - 1) when evaluating continuation
  // histories
  constexpr int extension = 5;
  SearchStack stack[MAX_PLY + extension] = {};
  SearchStack *ss = stack + extension - 1;

  for (int i = 4; i > 0; --i) {
    (ss - i)->ch = &continuationTable[false][NO_PIECE][A1][0];
    (ss - i)->staticEval = VAL_NONE;
  }

  for (int i = 0; i < MAX_PLY; i++)
    (ss + i)->ply = i;

  ss->pv = pv;

  size_t multiPV = sharedState.config.multiPV;
  multiPV = std::min(multiPV, rootMoves.size());

  while (++rootDepth < MAX_PLY and !threads.stop and
         !(limits.depth and isMainThread() and rootDepth > limits.depth)) {
    // Save previous iteration's scores
    for (RootMove &rm : rootMoves)
      rm.prevScore = rm.score;

    // MultiPV loop
    for (pvIdx = 0; pvIdx < multiPV; ++pvIdx)
      aspirationWindows(ss, alpha, beta, delta, bestValue);

    if (!threads.stop)
      completedDepth = rootDepth;

    // If mate is unproven, revert to the last best pv and score
    if (threads.abortedSearch && rootMoves[0].score != -VAL_INFINITE &&
        abs(rootMoves[0].score) <= VAL_MATE_BOUND) {
      rootMoves[0].pv = lastBestPV;
      rootMoves[0].score = lastBestMoveScore;
      // If the best move has changed, update the last best move
    } else if (rootMoves[0].pv[0] != lastBestPV[0]) {
      lastBestPV = rootMoves[0].pv;
      lastBestMoveScore = rootMoves[0].score;
      lastBestMoveDepth = rootDepth;
    }

    if (!isMainThread())
      continue;

    // Have we found a "mate in x"?
    if (limits.mate and abs(rootMoves[0].score) >= VAL_MATE_BOUND and
        VAL_MATE - abs(rootMoves[0].score) <= 2 * limits.mate)
      threads.stop = true;

    if (limits.useTimeManagement() and !threads.stop and
        checkTM(lastBestMoveDepth, pvStability, bestValue))
      threads.stop = true;
  }
}

void SearchWorker::aspirationWindows(SearchStack *ss, Value &alpha, Value &beta,
                                     Value &delta, Value &bestValue) {

  // Reset length of PV
  selDepth = 0;

  // Reset Aspiration Window starting size
  delta = 10;
  Value avg = rootMoves[pvIdx].averageScore;
  alpha = std::max(avg - delta, -VAL_MATE);
  beta = std::min(avg + delta, VAL_MATE);

  // Start with small window size, if fail high or low, increase window size
  int failedHighCnt = 0;

  // Aspiration Window loop
  while (true) {

    // Calculate adjusted depth, if fail high, reduce depth to focus on
    // other moves
    Depth adjustedDepth = std::max(1, rootDepth - failedHighCnt);
    rootDelta = beta - alpha;
    bestValue = search<ROOT>(rootPos, ss, adjustedDepth, alpha, beta, false);

    std::stable_sort(rootMoves.begin() + pvIdx, rootMoves.end());

    // If the search was stopped, break out of the loop
    if (threads.stop)
      break;

    // Fail Low (Reduce alpha and beta for a better window)
    if (bestValue <= alpha) {
      beta = (alpha + beta) / 2;
      alpha = std::max(bestValue - delta, -VAL_MATE);

      failedHighCnt = 0;
      // Fail High (Increase beta for a better window)
    } else if (bestValue >= beta) {
      beta = std::min(bestValue + delta, VAL_MATE);
      ++failedHighCnt;
      // Exact score returned, search is complete
    } else
      break;

    // Increase window size
    delta += delta / 3;
  }

  std::stable_sort(rootMoves.begin() + pvIdx, rootMoves.end());

  // Print PV if its the main thread, the loop is the last one or the node
  // count exceeds 10M, and the result isn't an unproven mate search
  if (isMainThread() and
      (threads.stop || pvIdx + 1 == config.multiPV || nodes > 10000000) and
      !(threads.abortedSearch and abs(rootMoves[0].score) <= VAL_MATE_BOUND))
    getPV(*this, rootDepth);
}

template <NodeType nodeType>
Value SearchWorker::search(Position &pos, SearchStack *ss, Depth depth,
                           Value alpha, Value beta, bool cutNode) {

  constexpr bool pvNode = nodeType != CUT;
  constexpr bool rootNode = nodeType == ROOT;

  // std::cout << depth << std::endl;

  // Step 1: if depth is 0, go into quiescence search
  if (depth <= 0)
    return qSearch < pvNode ? PV : CUT > (pos, ss, alpha, beta);

  // Limit depth if there is a extension explosion
  depth = std::min(depth, Depth(MAX_PLY - 1));

  // Step 3: Start Main Search (Initialise variables)
  Move pv[MAX_PLY + 1];
  BoardState st;

  Key hashKey;
  Move move, excludedMove, bestMove;
  Depth extension, newDepth;
  Value bestValue, value, eval, maxValue;
  bool givesCheck, improving, prevIsCapture;
  bool capture, ttCapture, opponentWorsening;
  int moveCount;
  PieceType movedPiece;

  std::vector<Move> capturesSearched(32, Move::none());
  std::vector<Move> quietsSearched(32, Move::none());

  ss->inCheck = pos.isInCheck();
  prevIsCapture = pos.getCaptured();
  Colour us = pos.getSideToMove();
  ss->moveCount = 0;
  bestValue = -VAL_INFINITE;
  maxValue = VAL_INFINITE;

  // Check for available remaining time
  checkTime();

  if (pvNode and selDepth < ss->ply + 1)
    selDepth = ss->ply + 1;

  if (!rootNode) {
    // Step 3: Check for aborted search and immediate draw
    if (threads.stop.load(std::memory_order_relaxed) or pos.isDraw(ss->ply) or
        ss->ply >= MAX_PLY) {
      return (ss->ply >= MAX_PLY && !ss->inCheck) ? Eval::evaluate(pos)
                                                  : valueDraw(nodes);
    }

    // Step 4: Mate distance pruning, if a shorter mate was found upward in the
    // tree, then there is no way to beat the alpha score. Same thing
    // happens for beta but reversed.
    alpha = std::max(matedIn(ss->ply), alpha);
    beta = std::min(mateIn(ss->ply + 1), beta);

    // If window is invalid, return alpha
    if (alpha >= beta)
      return alpha;
  }

  bestMove = Move::none();
  Square prevSq = (ss - 1)->currentMove ? (ss - 1)->currentMove.to() : NO_SQ;

  // Step 5: Check for a transposition table hit
  excludedMove = ss->excludedMove;
  hashKey = pos.getKey();
  auto [ttHit, ttData, ttWriter] = tt.probe(hashKey);
  ss->ttHit = ttHit;
  // If root node, then the tt move would the best move, else the move would be
  // the table move if available
  ttData.move = rootNode ? rootMoves[pvIdx].pv[0]
                : ttHit  ? ttData.move
                         : Move::none();
  // If there is a table hit then store the value from the table
  ttData.value =
      ttHit ? TTable::valueFromTT(ttData.value, ss->ply, pos.getFiftyMove())
            : VAL_NONE;
  // Set ttPV to true if there is a table hit and the move is a PV move
  ss->ttPV = excludedMove ? ss->ttPV : pvNode || (ttHit and ttData.isPV);
  // Set ttCapture to true if there is a table hit and the move is a capture
  ttCapture = ttData.move and pos.isCapture(ttData.move);

  // Check for early tt cutoff at non pv nodes
  if (!pvNode and !excludedMove and
      ttData.depth >= depth - (ttData.value <= beta) and
      ttData.value != VAL_NONE and
      (ttData.flag & ttData.value >= beta ? BOUND_LOWER : BOUND_UPPER)) {

    // if table move cutoff is only valid when the fifty move rule is violated
    // then there should not be a cutoff, therefore don't produce a
    // transposition table cutoff
    if (pos.getFiftyMove() < 90)
      return ttData.value;
  }

  // Step 6: Static evaluation
  eval = ss->staticEval = ss->inCheck ? VAL_NONE
                          : (ttHit and ttData.eval != VAL_NONE)
                              ? ttData.eval
                              : Eval::evaluate(pos);

  // Set up improving flag (if the static evaluation is better than the
  // previous)
  improving = ss->staticEval > (ss - 2)->staticEval;
  // Set up the opponent Worsening flag (If the static evaluation is shifting in
  // our favour)
  opponentWorsening = ss->staticEval + (ss - 1)->staticEval > 2;

  // Reset killer moves
  killerTable[ss->ply][0] = Move::none();
  killerTable[ss->ply][1] = Move::none();

  // Step 7: Internal Iterative Deepening
  if (depth >= 3 and pvNode and !ttData.move)
    depth -= 3;

  // Step 8: Check for quiescence search again
  if (depth <= 0)
    return qSearch<PV>(pos, ss, alpha, beta);

  // // Step 9: Cut Node depth reduction if the position is not really
  // considered
  // // before or is bad
  if (cutNode and depth >= 7 and (!ttData.move or ttData.flag == BOUND_LOWER))
    depth -= 2;

  const ContinuationHistory *ch[] = {(ss - 1)->ch, (ss - 2)->ch, (ss - 3)->ch,
                                     (ss - 4)->ch};

  MovePicker mp(pos, GenMove(ttData.move), depth, &killerTable,
                &counterMoveTable, &historyTable, &captureHistoryTable, ch,
                ss->ply);

  value = bestValue;

  moveCount = 0;

  // Step 10: Loop through all moves until no moves remain the move fail's high
  while ((move = mp.selectNext()) != Move::none()) {

    if (!pos.isPseudoLegal(move) || !pos.isLegal(move)) {
      pos.print();
      Colour us = pos.getSideToMove();
      Square from = move.from();
      Square to = move.to();
      Piece piece = pos.getPiece(from);
      printBitboard(pos.state()->pinned[us]);
      printBitboard(pos.state()->checkMask);

      std::cout << "GenStage: " << mp.stage << std::endl;

      MoveList<CAPTURES> moves(pos);

      for (Move m : moves)
        std::cout << move2Str(m) << std::endl;

      std::cout << "From: " << sq2Str(from) << std::endl;
      std::cout << "To: " << sq2Str(to) << std::endl;
      std::cout << "Piece: " << piece2Str(piece) << std::endl;
      std::cout << "Is legal: " << pos.isLegal(move) << std::endl;
      std::cout << "Is pseudo legal: " << pos.isPseudoLegal(move) << std::endl;
      std::cout << "Illegal move: " << move2Str(move) << std::endl;

      // If from square is not occupied by a piece belonging to the side to move
      if (piece == NO_PIECE || colourOf(piece) != us)
        std::cout << "Problem 1\n";

      // If destination cannot be occupied by the piece
      if (pos.getOccupiedBB(us) & to)
        std::cout << "Problem 2\n";

      // Handle pawn moves
      if (pieceTypeOf(piece) == PAWN) {
        bool isCapture = pawnAttacksBB(us, from) & pos.getOccupiedBB(~us) & to;
        bool isSinglePush =
            (from + pawnPush(us) == to) && !(pos.getOccupiedBB() & to);
        bool isDoublePush = (from + 2 * pawnPush(us) == to) &&
                            (rankOf(from) == relativeRank(us, RANK_2)) &&
                            !(pos.getOccupiedBB() & to) &&
                            !(pos.getOccupiedBB() & (to - pawnPush(us)));

        if (move.is<PROMOTION>() and move.promoted() == NO_PIECE_TYPE)
          std::cout << "Problem 3\n";

        if (move.is<EN_PASSANT>() &&
            !(pawnAttacksBB(us, from) & pos.state()->enPassant))
          std::cout << "Problem 4\n";

        if (move.is<NORMAL>() && !isCapture && !isSinglePush && !isDoublePush)
          std::cout << "Problem 5\n";
      }
      // If destination square is unreachable by the piece
      else if (!(attacksBB(pieceTypeOf(piece), from, pos.getOccupiedBB()) & to))
        std::cout << "Problem 6\n";

      if (pos.isInCheck()) {
        if (pieceTypeOf(piece) != KING) {
          if (!(pos.state()->checkMask & to))
            std::cout << "Problem 7\n";
        } else if (pos.state()->kingBan & to)
          std::cout << "Problem 8\n";
      }

      throw std::runtime_error("Illegal move");
    }

    if (move == excludedMove)
      continue;

    // If the mode is search move, then don't search the moves we are not here
    if (rootNode and !std::count(rootMoves.begin(), rootMoves.end(), move))
      continue;

    ss->moveCount = ++moveCount;

    if (pvNode) // Reset the pv if the next
      (ss + 1)->pv = nullptr;

    capture = pos.isCapture(move);
    movedPiece = pos.movedPiece(move);
    // givesCheck = pos.givesCheck(move);

    newDepth = depth - 1;

    ss->currentMove = move;
    ss->ch = &continuationTable[ss->inCheck][capture][movedPiece][move.to()];
    U64 nodeCount = rootNode ? nodes.load() : 0ULL;

    // Step 11: Make the move
    nodes.fetch_add(1, std::memory_order_relaxed);
    pos.makeMove(move, st);

    // Step 12: Late Move Reduction (LMR)
    if (depth >= 2 and moveCount > 1 + rootNode) {

      Depth d = std::max(Depth(1), newDepth);
      // Search with a extremely small window size to try to see if the moveis
      // good or not
      value = -search<CUT>(pos, ss + 1, d, -alpha - 1, -alpha, true);
      // Step 13. Full-depth search when LMR is skipped
    } else if (!pvNode || moveCount > 1) {
      value = -search<CUT>(pos, ss + 1, newDepth, -alpha - 1, -alpha, true);
    }

    // Step 14: For pv nodes only, do a full search on the first move or after a
    // fail high from LMR
    if (pvNode and (moveCount == 1 or value > alpha)) {
      // Update pv for the next move
      (ss + 1)->pv = pv;
      (ss + 1)->pv[0] = Move::none();

      value = -search<PV>(pos, ss + 1, newDepth, -beta, -alpha, false);
    }

    pos.unmakeMove();

    if (threads.stop.load(std::memory_order_relaxed))
      return VAL_ZERO;

    // Step 15: Update best move if the value is better
    if (rootNode) {
      RootMove &rm = *std::find(rootMoves.begin(), rootMoves.end(), move);

      rm.effort += nodes - nodeCount;

      rm.averageScore = rm.averageScore != -VAL_INFINITE
                            ? (rm.averageScore + value) / 2
                            : value;

      // PV move or new best move
      if (moveCount == 1 || value > alpha) {
        rm.score = value;
        rm.selDepth = selDepth;

        rm.pv.resize(1);

        for (Move *m = (ss + 1)->pv; *m != Move::none(); ++m)
          rm.pv.push_back(*m);

      } else {
        rm.score = -VAL_INFINITE;
      }
    }

    // See if the alternative move that is equal in eval to the best move,
    // Randomly choose between the two, by pretending one is better than the
    // other (Check if the new value exceeds the mate bound or if the search
    // is deep enough to consider the move)
    int inc =
        (value == bestValue and nodes & 0x2 == 0 and
         ss->ply + 2 >= rootDepth and std::abs(value) + 1 < VAL_MATE_BOUND);

    if (value + inc > bestValue) {
      bestValue = value;
      bestMove = move;

      if (value + inc > alpha) {

        if (pvNode and !rootNode) // Update the PV
          updatePV(ss->pv, move, (ss + 1)->pv);

        if (value >= beta)
          break;
        else
          alpha = value;
      }
    }

    // Update move lists
    if (move != bestMove and moveCount <= 32) {
      if (capture)
        capturesSearched.push_back(move);
      else
        quietsSearched.push_back(move);
    }
  }

  // Step 16: Check for mate and stalemate
  if (!moveCount)
    bestValue = excludedMove  ? alpha
                : ss->inCheck ? matedIn(ss->ply)
                              : VAL_ZERO;
  else if (bestMove)
    updateAllStats(pos, ss, bestMove, prevSq, quietsSearched, capturesSearched,
                   depth);
  else if (ttData.move)
    updateQuietHistories(pos, ss, ttData.move, statBonus(depth) / 4);

  // If no good move is found and the previous move was ttPV, then the
  // previous opponent move is probably good, so we should store that in the
  // table.
  if (bestValue <= alpha)
    ss->ttPV = ss->ttPV || ((ss - 1)->ttPV and depth > 3);

  if (!excludedMove and !(rootNode and pvIdx))
    ttWriter.write(hashKey, TTable::valueToTT(bestValue, ss->ply), ss->ttPV,
                   bestValue >= beta     ? BOUND_LOWER
                   : pvNode and bestMove ? BOUND_EXACT
                                         : BOUND_UPPER,
                   depth, bestMove, ss->staticEval, tt.gen8);

  return bestValue;
}

template <NodeType nodeType>
Value SearchWorker::qSearch(Position &pos, SearchStack *ss, Value alpha,
                            Value beta) {

  constexpr bool pvNode = nodeType == PV;
  Move pv[MAX_PLY + 1];
  BoardState st;
  Key hashKey;
  Move move, bestMove;
  Value bestValue, value, eval;
  bool pvHit, givesCheck, capture;
  int moveCount;
  PieceType movedPiece;
  Colour us = pos.getSideToMove();

  // Step 1: Initialise child PV
  if (pvNode) {
    ss->pv = pv;
    ss->pv[0] = Move::none();
  }

  bestMove = Move::none();
  ss->inCheck = pos.isInCheck();
  moveCount = 0;

  // Update selDepth
  if (pvNode and selDepth < ss->ply + 1)
    selDepth = ss->ply + 1;

  // Step 2: Check for draw, if draw, return draw value
  if (pos.isDraw(ss->ply) or ss->ply >= MAX_PLY)
    return (ss->ply >= MAX_PLY && !ss->inCheck) ? Eval::evaluate(pos)
                                                : valueDraw(nodes);

  // Step 3: Transposition Table Probe
  hashKey = pos.getKey();
  auto [ttHit, ttData, ttWriter] = tt.probe(hashKey);
  ss->ttHit = ttHit;
  ttData.move = ttHit ? ttData.move : Move::none();
  ttData.value =
      ttHit ? TTable::valueFromTT(ttData.value, ss->ply, pos.getFiftyMove())
            : VAL_NONE;
  pvHit = ttHit and ttData.isPV;

  if (!pvNode and ttData.depth >= 0 and ttData.value != VAL_NONE and
      (ttData.flag & (ttData.value >= beta ? BOUND_LOWER : BOUND_UPPER)))
    return ttData.value;

  // Step 4: Static Evaluation
  eval = ss->staticEval =
      (ttHit and ttData.eval != VAL_NONE) ? ttData.eval : Eval::evaluate(pos);

  if (!ttHit and !ss->inCheck)
    ttWriter.write(hashKey, VAL_NONE, false, BOUND_NONE, 0, Move::none(), eval,
                   tt.gen8);

  // Step 5: Check for stand pat
  bestValue = eval;
  alpha = std::max(alpha, eval);
  if (alpha >= beta)
    return eval;

  const ContinuationHistory *ch[] = {(ss - 1)->ch, (ss - 2)->ch};

  Square prevSq = (ss - 1)->currentMove ? (ss - 1)->currentMove.to() : NO_SQ;

  // Step 7: Main Loop
  MovePicker mp(pos, GenMove(ttData.move), 0, &killerTable, &counterMoveTable,
                &historyTable, &captureHistoryTable, ch, ss->ply);

  while ((move = mp.selectNext()) != Move::none()) {

    if (!pos.isPseudoLegal(move) || !pos.isLegal(move)) {
      pos.print();
      Colour us = pos.getSideToMove();
      Square from = move.from();
      Square to = move.to();
      Piece piece = pos.getPiece(from);
      printBitboard(pos.state()->pinned[us]);
      printBitboard(pos.state()->checkMask);

      std::cout << "GenStage: " << mp.stage << std::endl;

      MoveList<CAPTURES> moves(pos);

      for (Move m : moves)
        std::cout << move2Str(m) << std::endl;

      std::cout << "From: " << sq2Str(from) << std::endl;
      std::cout << "To: " << sq2Str(to) << std::endl;
      std::cout << "Piece: " << piece2Str(piece) << std::endl;
      std::cout << "Is legal: " << pos.isLegal(move) << std::endl;
      std::cout << "Is pseudo legal: " << pos.isPseudoLegal(move) << std::endl;
      std::cout << "Illegal move: " << move2Str(move) << std::endl;

      // If from square is not occupied by a piece belonging to the side to move
      if (piece == NO_PIECE || colourOf(piece) != us)
        std::cout << "Problem 1\n";

      // If destination cannot be occupied by the piece
      if (pos.getOccupiedBB(us) & to)
        std::cout << "Problem 2\n";

      // Handle pawn moves
      if (pieceTypeOf(piece) == PAWN) {
        bool isCapture = pawnAttacksBB(us, from) & pos.getOccupiedBB(~us) & to;
        bool isSinglePush =
            (from + pawnPush(us) == to) && !(pos.getOccupiedBB() & to);
        bool isDoublePush = (from + 2 * pawnPush(us) == to) &&
                            (rankOf(from) == relativeRank(us, RANK_2)) &&
                            !(pos.getOccupiedBB() & to) &&
                            !(pos.getOccupiedBB() & (to - pawnPush(us)));

        if (move.is<PROMOTION>() and move.promoted() == NO_PIECE_TYPE)
          std::cout << "Problem 3\n";

        if (move.is<EN_PASSANT>() &&
            !(pawnAttacksBB(us, from) & pos.state()->enPassant))
          std::cout << "Problem 4\n";

        if (move.is<NORMAL>() && !isCapture && !isSinglePush && !isDoublePush)
          std::cout << "Problem 5\n";
      }
      // If destination square is unreachable by the piece
      else if (!(attacksBB(pieceTypeOf(piece), from, pos.getOccupiedBB()) & to))
        std::cout << "Problem 6\n";

      if (pos.isInCheck()) {
        if (pieceTypeOf(piece) != KING) {
          if (!(pos.state()->checkMask & to))
            std::cout << "Problem 7\n";
        } else if (pos.state()->kingBan & to)
          std::cout << "Problem 8\n";
      }

      throw std::runtime_error("Illegal move");
    }

    capture = pos.isCapture(move);

    moveCount++;

    // Step 8: Pruning (TODO)

    ss->currentMove = move;
    movedPiece = pos.movedPiece(move);
    ss->ch = &continuationTable[ss->inCheck][capture][movedPiece][move.to()];

    // Step 9: Make and search move
    nodes.fetch_add(1, std::memory_order_relaxed);
    pos.makeMove(move, st);
    value = -qSearch<nodeType>(pos, ss + 1, -beta, -alpha);
    pos.unmakeMove();

    // Step 10: Check for new best move
    if (value > bestValue) {
      bestValue = value;
      bestMove = move;

      if (value > alpha) {
        alpha = value;

        if (pvNode)
          updatePV(ss->pv, move, (ss + 1)->pv);

        if (value >= beta)
          break;
      }
    }
  }

  // Step 11: Check for mate and stalemate
  if (ss->inCheck and bestValue == -VAL_INFINITE)
    return matedIn(ss->ply);

  ttWriter.write(hashKey, TTable::valueToTT(bestValue, ss->ply), pvHit,
                 bestValue >= beta ? BOUND_LOWER : BOUND_UPPER, 0, bestMove,
                 ss->staticEval, tt.gen8);

  return bestValue;
}

void SearchWorker::updateAllStats(const Position &pos, SearchStack *ss,
                                  Move bestMove, Square prevSq,
                                  std::vector<Move> &quietsSearched,
                                  std::vector<Move> &capturesSearched,
                                  Depth depth) {
  PieceType movedPiece = pos.movedPiece(bestMove);
  PieceType captured;
  int bonus = statBonus(depth);

  const Square from = bestMove.from();
  const Square to = bestMove.to();

  if (!pos.isCapture(bestMove)) {
    updateQuietHistories(pos, ss, bestMove, bonus);

    updateKillerMoves(bestMove, ss->ply);
    updateCounterMoves(pos, bestMove, prevSq);

    for (Move m : quietsSearched)
      updateQuietHistories(pos, ss, m, -bonus);
  } else
    updateCaptureHistories(pos, ss, bestMove, bonus);

  // Extra penalty for early quiet move that is not a tt move in previous ply
  if (prevSq != NO_SQ and (ss - 1)->moveCount == 1 + (ss - 1)->ttHit and
      !pos.getCaptured())
    updateContinuationHistories(ss - 1, pos.getPieceType(prevSq), prevSq,
                                -bonus);

  for (Move m : capturesSearched)
    updateCaptureHistories(pos, ss, m, -bonus);
}

void SearchWorker::updateQuietHistories(const Position &pos, SearchStack *ss,
                                        Move move, int bonus) {

  const Square from = move.from();
  const Square to = move.to();

  Colour us = pos.getSideToMove();

  const bool threatFrom = pos.state()->attacked & from;
  const bool threatTo = pos.state()->attacked & to;

  historyTable[us][threatFrom][threatTo][from][to] << bonus;

  updateContinuationHistories(ss, pos.movedPiece(move), to, bonus);
}

void SearchWorker::updateKillerMoves(Move move, int ply) {
  if (move != killerTable[ply][0])
    killerTable[ply][1] = killerTable[ply][0];
  killerTable[ply][0] = move;
}

void SearchWorker::updateCounterMoves(const Position &pos, Move move,
                                      Square prevSq) {
  const Square to = move.to();
  const PieceType pt = pos.getPieceType(move.from());

  counterMoveTable[~pos.getSideToMove()][pt][to] = move;
}

void SearchWorker::updateCaptureHistories(const Position &pos, SearchStack *ss,
                                          Move move, int bonus) {
  const Square to = move.to();
  const Square from = move.from();

  const PieceType captured = pos.captured(move);
  const PieceType piece = pos.movedPiece(move);

  const bool threatFrom = pos.state()->attacked & from;
  const bool threatTo = pos.state()->attacked & to;

  captureHistoryTable[piece][threatFrom][threatTo][to][captured] << bonus;
}

void SearchWorker::updateContinuationHistories(SearchStack *ss, PieceType pt,
                                               Square to, int bonus) {
  for (int i : {1, 2, 3, 4}) {
    // Only update the first two continuation if we are in check, as
    // the continuations would be easily broken in this scenario,
    // making the scores useless
    if (ss->inCheck and i > 2)
      break;
    if ((ss - i)->currentMove)
      (*(ss - i)->ch)[pt][to] << bonus;
  }
}

} // namespace Maestro
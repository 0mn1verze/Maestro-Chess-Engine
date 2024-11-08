
#include <iostream>
#include <math.h>

#include "eval.hpp"
#include "hash.hpp"
#include "search.hpp"
#include "thread.hpp"
#include "uci.hpp"

namespace Maestro {

struct Config;

int LMRTable[64][64];
int LateMovePruningCounts[2][11];

void initLMR() {

  // Init Late Move Reductions Table
  for (int depth = 1; depth < 64; depth++)
    for (int played = 1; played < 64; played++)
      LMRTable[depth][played] = -0.2156 + log(depth) * log(played) / 2.4696;

  for (int depth = 1; depth <= 10; depth++) {
    LateMovePruningCounts[0][depth] = 2.0767 + 0.3743 * depth * depth;
    LateMovePruningCounts[1][depth] = 3.8733 + 0.7124 * depth * depth;
  }
}

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

  while (!threads.stop && limits.infinite) {
  }

  threads.stop = true;

  threads.waitForThreads();

  SearchWorker *best = this;

  if (sharedState.config.multiPV == 1 && !limits.depth && !limits.mate &&
      rootMoves[0].pv[0] != Move::none())
    best = threads.getBestThread()->worker.get();

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

  // Scale time between 80% && 120% , based on stability of best moves
  const double pvFactor = 1.2 - 0.04 * pvStability;

  // Scale time between 80% && 120% , based on score fluctuation
  const double scoreFluctuation = abs(bestPreviousAvgScore - bestValue);
  const double scoreFactor = 1.20 - 0.04 * std::min(scoreFluctuation, 10.00);

  // Scale time between 50% && 240%, based on what % of nodes are used for
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
  if (isMainThread() && completedDepth >= 1 &&
      ((limits.useTimeManagement() && tm.elapsed() >= tm.maximum()) ||
       (limits.movetime && tm.elapsed() >= limits.movetime)))
    threads.stop = threads.abortedSearch = true;
}

void SearchWorker::iterativeDeepening() {

  Move pv[MAX_PLY + 1];

  Depth lastBestMoveDepth = 0;
  Value lastBestMoveScore = -VAL_INFINITE;
  auto lastBestPV = std::vector<Move>{Move::none()};

  Value bestValue = -VAL_INFINITE;
  Colour us = rootPos.getSideToMove();
  int pvStability = 0;
  int delta = 0;

  // // Allocate search stack
  // // (ss - 3) is accessed from (ss - 1) when evaluating continuation
  // histories
  constexpr int EXTENSION = 4;
  SearchStack stack[MAX_PLY + EXTENSION + 1] = {};
  SearchStack *ss = stack + EXTENSION;

  for (int i = EXTENSION; i > 0; --i) {
    (ss - i)->ch = &continuationTable[false][0][NO_PIECE][NO_SQ];
    (ss - i)->staticEval = VAL_NONE;
  }

  for (int i = 0; i <= MAX_PLY; i++)
    (ss + i)->ply = i;

  ss->pv = pv;

  size_t multiPV = sharedState.config.multiPV;
  multiPV = std::min(multiPV, rootMoves.size());

  while (++rootDepth < MAX_PLY && !threads.stop &&
         !(limits.depth && isMainThread() && rootDepth > limits.depth)) {
    // Save previous iteration's scores
    for (RootMove &rm : rootMoves)
      rm.prevScore = rm.score;

    // MultiPV loop
    for (pvIdx = 0; pvIdx < multiPV; ++pvIdx)
      searchPosition(ss, bestValue);

    if (!threads.stop)
      completedDepth = rootDepth;

    // If mate is unproven, revert to the last best pv && score
    if (threads.abortedSearch && rootMoves[0].score != -VAL_INFINITE &&
        rootMoves[0].score <= VAL_MATE_BOUND) {
      Utility::moveToFront(
          rootMoves, [&lastBestPV = std::as_const(lastBestPV)](const auto &rm) {
            return rm == lastBestPV[0];
          });
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
    if (limits.mate && abs(rootMoves[0].score) >= VAL_MATE_BOUND &&
        VAL_MATE - abs(rootMoves[0].score) <= 2 * limits.mate)
      threads.stop = true;

    if (limits.useTimeManagement() && !threads.stop &&
        (checkTM(lastBestMoveDepth, pvStability, bestValue) ||
         tm.elapsed() >= tm.maximum()))
      threads.stop = threads.abortedSearch = true;
  }
}

void SearchWorker::searchPosition(SearchStack *ss, Value &bestValue) {

  // Reset length of PV
  selDepth = 0;

  failHigh = 0;
  failHighFirst = 0;

  bestValue =
      search<ROOT>(rootPos, ss, rootDepth, -VAL_INFINITE, VAL_INFINITE, false);

  // If the search was stopped, break out of the loop
  if (threads.stop)
    return;

  std::stable_sort(rootMoves.begin(), rootMoves.end());

  // Print PV if its the main thread, the loop is the last one or the node
  // count exceeds 10M, && the result isn't an unproven mate search
  if (isMainThread() &&
      (threads.stop || pvIdx + 1 == config.multiPV || nodes > 10000000) &&
      !(threads.abortedSearch && rootMoves[0].score <= VAL_MATE_BOUND))
    getPV(*this, rootDepth);
}

template <NodeType nodeType>
Value SearchWorker::search(Position &pos, SearchStack *ss, Depth depth,
                           Value alpha, Value beta, bool cutNode) {

  // Initialize variables
  constexpr bool pvNode = nodeType != NON_PV;
  constexpr bool rootNode = nodeType == ROOT;
  const bool allNode = !(pvNode || cutNode);

  // Quiescence search
  if (depth <= 0)
    return qSearch < pvNode ? PV : NON_PV > (pos, ss, alpha, beta);

  // Limit search depth to prevent search explosion
  depth = std::min(depth, MAX_PLY - 1);

  // State variables
  Move pv[MAX_PLY + 1];
  BoardState st;

  Key hashKey;
  Move move, bestMove, excludedMove;
  Value bestValue, value, hist, probCutBeta;
  Depth newDepth, R;
  bool givesCheck, improving, prevCapture, oppWorsening;
  bool isCapture, ttCapture;
  PieceType movedPieceType;
  Square prevSq;
  int moveCount;

  std::vector<Move> quietsSearched(32);
  std::vector<Move> capturesSearched(32);

  // Initialise node
  Colour us = pos.getSideToMove();
  moveCount = 0;
  excludedMove = ss->excludedMove;
  ss->inCheck = pos.isInCheck();
  ss->moveCount = 0;
  prevCapture = pos.getCaptured();
  bestValue = -VAL_INFINITE;

  // Check for remaining time
  if (isMainThread())
    checkTime();

  // Update node count
  nodes.fetch_add(1, std::memory_order_relaxed);

  // Update seldepth to sent to GUI
  if (pvNode)
    selDepth = std::max(selDepth, ss->ply + 1);

  if (!rootNode) {
    // Check for aborted search or immediate draw
    if (threads.stop.load(std::memory_order_relaxed) || pos.isDraw(ss->ply) ||
        ss->ply >= MAX_PLY)
      return (ss->ply >= MAX_PLY && !ss->inCheck) ? Eval::evaluate(pos)
                                                  : valueDraw(nodes);

    // Mate distance pruning
    alpha = std::max(matedIn(ss->ply), alpha);
    beta = std::min(mateIn(ss->ply + 1), beta);

    if (alpha >= beta)
      return alpha;
  }

  bestMove = Move::none();
  prevSq = ((ss - 1)->currentMove) ? (ss - 1)->currentMove.to() : NO_SQ;

  // Transposition Table Lookup
  hashKey = pos.getKey();
  auto [ttHit, ttData, ttWriter] = tt.probe(hashKey);
  // Processing TT data
  ss->ttHit = ttHit;
  ttData.move = rootNode ? rootMoves[pvIdx].pv[0]
                : ttHit  ? ttData.move
                         : Move::none();
  ttData.value =
      ttHit ? TTable::valueFromTT(ttData.value, ss->ply, pos.getFiftyMove())
            : VAL_NONE;
  ss->ttPV = excludedMove ? ss->ttPV : pvNode || (ttHit && ttData.isPV);
  ttCapture = ttData.move && pos.isCapture(ttData.move);

  // If there is a excluded move (In singular extension search) then go to
  // static eval
  if (!pvNode && !excludedMove && ttData.depth > depth - 1 &&
      ttData.value != VAL_NONE &&
      (ttData.flag & (ttData.value >= beta ? FLAG_LOWER : FLAG_UPPER)) &&
      pos.getFiftyMove() < 90) {

    if (ttData.move && ttData.value >= beta) {
      if (!ttCapture)
        updateQuietHistory(pos, ss, ttData.move, statBonus(depth));
      if (prevSq != NO_SQ && (ss - 1)->moveCount <= 2 && !prevCapture)
        updateContinuationHistory(ss - 1, pos.getPieceType(prevSq), prevSq,
                                  -statBonus(depth + 1));
    }

    return ttData.value;
  }

  // Static Evaluation
  if (ss->inCheck) {
    ss->staticEval = (ss - 2)->staticEval;
    improving = false;
    goto moves_loop;
  } else if (excludedMove) {

  } else if (ss->ttHit) {
    ss->staticEval =
        ttData.eval != VAL_NONE ? ttData.eval : Eval::evaluate(pos);
  } else {
    ss->staticEval = Eval::evaluate(pos);

    ttWriter.write(hashKey, VAL_NONE, ss->ttPV, FLAG_NONE, DEPTH_UNSEARCHED,
                   Move::none(), ss->staticEval, tt.gen8);
  }

  // Set improving flag (Whether the position is better than the previous one)
  improving = ss->staticEval > (ss - 2)->staticEval;

  // Set opponent worsening flag (Whether the opponent's position is worse than)
  oppWorsening = ss->staticEval + (ss - 1)->staticEval > 2;

  // Futility pruning (If eval is well enough, assume the eval will hold above
  // beta or cause a cutoff)
  if (!pvNode && !ss->inCheck && !excludedMove && depth <= 8 &&
      ss->staticEval - 65 * std::max(0, depth - improving) >= beta)
    return ss->staticEval;

  // Null Move Pruning
  if (cutNode && (ss - 1)->currentMove != Move::null() && depth >= 2 &&
      ss->staticEval >= beta - 23 * depth + 400 && !excludedMove &&
      pos.getNonPawnMaterial(us) && beta > VAL_MATE_BOUND) {

    R = std::min(int(ss->staticEval - beta) / 209, 6) + depth / 3 + 5;

    ss->currentMove = Move::null();
    ss->ch = &continuationTable[0][0][NO_PIECE][A1];

    pos.makeNullMove(st);
    // Prefetch the next entry in the TT
    TTable::prefetch(tt.firstEntry(pos.getKey()));

    Value nullValue =
        -search<NON_PV>(pos, ss + 1, depth - R, -beta, -beta + 1, false);

    pos.unmakeNullMove();

    if (nullValue >= beta && nullValue < VAL_MATE_BOUND)
      return nullValue;
  }
  // Internal iterative deepening
  if (pvNode && !ttData.move)
    depth -= 3;

  // Check for quiescence search
  if (depth <= 0)
    return qSearch<PV>(pos, ss, alpha, beta);

  if (cutNode && depth >= 7 and (!ttData.move || ttData.flag == FLAG_UPPER))
    depth -= 2;

  // Probcut
  // If we have a good enough capture or queen promotion, and a previoussearch
  // returns a value a lot high than beta, we can safely prune the move
  probCutBeta = beta + 200 - 50 * improving - 30 * oppWorsening;
  if (!pvNode && depth > 3 && std::abs(beta) < VAL_MATE_BOUND &&
      !(ttData.depth >= depth - 3 && ttData.value != VAL_NONE &&
        ttData.value < probCutBeta)) {

    MovePicker mp(pos, ttData.move, &captureHistoryTable,
                  probCutBeta - ss->staticEval);
    Piece captured;

    while ((move = mp.selectNext()) != Move::none()) {

      if (move == excludedMove)
        continue;

      ss->currentMove = move;
      ss->ch = &continuationTable[ss->inCheck][true][pos.movedPieceType(move)]
                                 [move.to()];

      pos.makeMove(move, st);

      // Perform a preliminary qsearch to verify that the move holds
      value = -qSearch<NON_PV>(pos, ss + 1, -probCutBeta, -probCutBeta + 1);

      // If the qsearch held, perform the regular search
      if (value >= probCutBeta)
        value = -search<NON_PV>(pos, ss + 1, depth - 4, -probCutBeta,
                                -probCutBeta + 1, !cutNode);

      pos.unmakeMove(move);

      if (value >= probCutBeta) {
        updateCaptureHistory(pos, ss, move, statBonus(depth - 2));

        ttWriter.write(hashKey, TTable::valueToTT(value, ss->ply), ss->ttPV,
                       FLAG_LOWER, depth - 3, move, ss->staticEval, tt.gen8);

        return std::abs(value) <= VAL_MATE_BOUND ? value - (probCutBeta - beta)
                                                 : value;
      }
    }
  }

moves_loop:
  // Continuation history
  const ContinuationHistory *ch[] = {(ss - 1)->ch, (ss - 2)->ch, (ss - 3)->ch,
                                     (ss - 4)->ch};
  // MovePicker
  MovePicker mp(*this, pos, ss, Move::none(), ch, depth);

  value = bestValue;

  while ((move = mp.selectNext()) != Move::none()) {
    // Skip excluded move
    if (move == excludedMove)
      continue;

    // For the searchmoves option, skip moves not in the list.
    // For the multipv option, skip searched nodes.
    if (rootNode &&
        !std::count(rootMoves.begin() + pvIdx, rootMoves.end(), move))
      continue;

    // Update move count
    ss->moveCount = ++moveCount;

    // Clear stack pvs
    if (pvNode)
      (ss + 1)->pv = nullptr;

    isCapture = pos.isCapture(move);
    movedPieceType = pos.movedPieceType(move);
    givesCheck = pos.givesCheck(move);

    hist =
        isCapture ? getCaptureHistory(pos, move) : getQuietHistory(pos, move);

    // Calculate new depth
    newDepth = depth - 1;

    // Late move pruning
    if (!rootNode && pos.getNonPawnMaterial(us) && bestValue > VAL_MATE_BOUND) {

      if (depth <= 8 && moveCount >= LateMovePruningCounts[improving][depth])
        mp.skipQuietMoves();
    }

    // Update current move
    ss->currentMove = move;
    ss->ch =
        &continuationTable[ss->inCheck][isCapture][movedPieceType][move.to()];
    U64 nodeCount = rootNode ? U64(nodes) : 0;

    // Output current move to UCI
    if (rootNode && isMainThread() && tm.elapsed() > 2500)
      UCI::uciReportCurrentMove(depth, move, moveCount + pvIdx);

    // Make the move
    pos.makeMove(move, st);
    // Prefetch the next entry in the TT
    TTable::prefetch(tt.firstEntry(pos.getKey()));

    // Late move reduction
    if (depth >= 2 && moveCount > 1) {

      if (!isCapture) {
        R = LMRTable[std::min(63, depth)][std::min(63, moveCount)];

        R += ss->inCheck && movedPieceType == KING;

        R -= mp.stage < COUNTER_MOVE;

        R -= hist / 6000;
      } else {
        R = 2 - (hist / 5000);

        R -= givesCheck;
      }

      value = -search<NON_PV>(pos, ss + 1, std::max(1, newDepth - R),
                              -alpha - 1, -alpha, true);

      if (value > alpha && R > 1)
        value = -search<NON_PV>(pos, ss + 1, newDepth, -alpha - 1, -alpha,
                                !cutNode);
    } else if (!pvNode || moveCount > 1)
      value =
          -search<NON_PV>(pos, ss + 1, newDepth, -alpha - 1, -alpha, !cutNode);

    if (pvNode && (moveCount == 1 || value > alpha)) {
      // Set new PV
      (ss + 1)->pv = pv;
      (ss + 1)->pv[0] = Move::none();

      // Recursive search
      value = -search<PV>(pos, ss + 1, newDepth, -beta, -alpha, false);
    }

    // Unmake the move
    pos.unmakeMove(move);

    // Check for stop
    if (threads.stop.load(std::memory_order_relaxed))
      return VAL_ZERO;

    // Update best move
    if (rootNode) {

      RootMove &rm = *std::find(rootMoves.begin(), rootMoves.end(), move);

      rm.effort = nodes - nodeCount;

      // PV move or new best move
      if (moveCount == 1 || value > alpha) {
        rm.score = value;
        rm.selDepth = selDepth;

        rm.pv.resize(1);

        for (Move *m = (ss + 1)->pv; *m != Move::none(); ++m)
          rm.pv.push_back(*m);

      } else {
        // All other moves but the PV are set to the lowest value
        rm.score = -VAL_INFINITE;
      }
    }

    if (value > bestValue) {
      bestValue = value;

      if (value > alpha) {
        bestMove = move;

        if (pvNode && !rootNode)
          updatePV(ss->pv, move, (ss + 1)->pv);

        if (value >= beta) {
          if (moveCount == 1)
            failHighFirst++;
          failHigh++;
          break;
        }

        alpha = value;
      }
    }

    // Update searched moves list
    if (move != bestMove && moveCount <= 32) {
      if (isCapture)
        capturesSearched.push_back(move);
      else
        quietsSearched.push_back(move);
    }
  }

  // Check for mate and stalemate
  if (!moveCount)
    return ss->inCheck ? matedIn(ss->ply) : VAL_ZERO;
  else if (bestMove)
    updateAllStats(pos, ss, bestMove, prevSq, quietsSearched, capturesSearched,
                   depth);
  // Write to TT, save static eval
  if (!excludedMove && !(rootNode && pvIdx))
    ttWriter.write(hashKey, TTable::valueToTT(bestValue, ss->ply), ss->ttPV,
                   bestValue >= beta    ? FLAG_LOWER
                   : pvNode && bestMove ? FLAG_EXACT
                                        : FLAG_UPPER,
                   depth, bestMove, ss->staticEval, tt.gen8);

  return bestValue;
}

// Quiescence search function
template <NodeType nodeType>
Value SearchWorker::qSearch(Position &pos, SearchStack *ss, Value alpha,
                            Value beta) {
  constexpr bool pvNode = nodeType != NON_PV;
  constexpr bool rootNode = nodeType == ROOT;

  // Initialize variables
  Move pv[MAX_PLY + 1];
  BoardState st;

  Key hashKey;
  Move move, bestMove;
  Value bestValue, value;
  bool pvHit, givesCheck, isCapture;
  int moveCount;

  // Initialise node
  if (pvNode) {
    (ss + 1)->pv = pv;
    ss->pv[0] = Move::none();
  }

  Colour us = pos.getSideToMove();
  bestMove = Move::none();
  ss->inCheck = pos.isInCheck();
  moveCount = 0;
  // Update node count
  nodes.fetch_add(1, std::memory_order_relaxed);
  // Update seldepth to sent to GUI
  if (pvNode)
    selDepth = std::max(selDepth, ss->ply + 1);

  if (pos.isDraw(ss->ply) || ss->ply >= MAX_PLY)
    return (ss->ply >= MAX_PLY && !ss->inCheck) ? Eval::evaluate(pos)
                                                : VAL_DRAW;
  // Transposition Table Lookup
  hashKey = pos.getKey();
  auto [ttHit, ttData, ttWriter] = tt.probe(hashKey);
  // Processing TT data
  ss->ttHit = ttHit;
  ttData.move = ttHit ? ttData.move : Move::none();
  ttData.value =
      ttHit ? TTable::valueFromTT(ttData.value, ss->ply, pos.getFiftyMove())
            : VAL_NONE;
  pvHit = ttHit && ttData.isPV;

  // At non pv nodes, we check for early cutoffs
  if (!pvNode && ttData.depth >= DEPTH_QS && ttData.value != VAL_NONE &&
      (ttData.flag & (ttData.value >= beta ? FLAG_LOWER : FLAG_UPPER)))
    return ttData.value;

  // Static Evaluation
  if (ss->ttHit) {
    ss->staticEval = bestValue =
        ttData.eval != VAL_NONE ? ttData.eval : Eval::evaluate(pos);
  } else {
    ss->staticEval = bestValue = Eval::evaluate(pos);

    ttWriter.write(hashKey, VAL_NONE, false, FLAG_NONE, DEPTH_UNSEARCHED,
                   Move::none(), ss->staticEval, tt.gen8);
  }

  // Stand pat
  if (bestValue >= beta)
    return bestValue;

  if (bestValue > alpha)
    alpha = bestValue;

  // Continuation history
  const ContinuationHistory *ch[] = {(ss - 1)->ch, (ss - 2)->ch};

  // MovePicker
  MovePicker mp(*this, pos, ss, ttData.move, ch, DEPTH_QS);

  // Main loop
  while ((move = mp.selectNext()) != Move::none()) {
    givesCheck = pos.givesCheck(move);
    isCapture = pos.isCapture(move);

    moveCount++;

    // Update current move
    ss->currentMove = move;
    ss->ch = &continuationTable[ss->inCheck][isCapture]
                               [pos.movedPieceType(move)][move.to()];

    // Make the move
    pos.makeMove(move, st);
    // Prefetch the next entry in the TT
    TTable::prefetch(tt.firstEntry(pos.getKey()));

    // Recursive quiescence search
    value = -qSearch<nodeType>(pos, ss + 1, -beta, -alpha);

    // Unmake the move
    pos.unmakeMove(move);

    if (value > bestValue) {
      bestValue = value;

      if (value > alpha) {
        bestMove = move;

        if (pvNode && !rootNode)
          updatePV(ss->pv, move, (ss + 1)->pv);

        if (value >= beta) {
          if (moveCount == 1)
            failHighFirst++;
          failHigh++;
          break;
        }

        alpha = value;
      }
    }
  }

  ttWriter.write(hashKey, TTable::valueToTT(bestValue, ss->ply), pvHit,
                 bestValue >= beta ? FLAG_LOWER : FLAG_UPPER, DEPTH_QS,
                 bestMove, ss->staticEval, tt.gen8);

  return bestValue;
}

void SearchWorker::updateAllStats(const Position &pos, SearchStack *ss,
                                  Move bestMove, Square prevSq,
                                  std::vector<Move> &quietsSearched,
                                  std::vector<Move> &capturesSearched,
                                  Depth depth) {
  PieceType movedPieceType = pos.movedPieceType(bestMove);
  PieceType captured;
  int bonus = statBonus(depth);

  const Square from = bestMove.from();
  const Square to = bestMove.to();

  if (!pos.isCapture(bestMove)) {
    updateQuietHistory(pos, ss, bestMove, bonus);
    updateContinuationHistory(ss, pos.movedPieceType(bestMove), to, bonus);

    updateKillerMoves(bestMove, ss->ply);
    updateCounterMoves(pos, bestMove, prevSq);

    for (Move m : quietsSearched) {
      updateQuietHistory(pos, ss, m, -bonus);
      updateContinuationHistory(ss, pos.movedPieceType(m), to, -bonus);
    }
  } else
    updateCaptureHistory(pos, ss, bestMove, bonus);

  // Extra penalty for early quiet move that is not a tt move in previous ply
  if (prevSq != NO_SQ && (ss - 1)->moveCount == 1 + (ss - 1)->ttHit &&
      !pos.getCaptured())
    updateContinuationHistory(ss - 1, pos.getPieceType(prevSq), prevSq, -bonus);

  for (Move m : capturesSearched)
    updateCaptureHistory(pos, ss, m, -bonus);
}

void SearchWorker::updateQuietHistory(const Position &pos, SearchStack *ss,
                                      Move move, int bonus) {

  const Square from = move.from();
  const Square to = move.to();

  Colour us = pos.getSideToMove();

  const bool threatFrom = pos.state()->attacked & from;
  const bool threatTo = pos.state()->attacked & to;

  historyTable[us][threatFrom][threatTo][from][to] << bonus;
}

void SearchWorker::updateKillerMoves(Move move, int ply) {
  if (move != killerTable[ply][0]) {
    killerTable[ply][1] = killerTable[ply][0];
    killerTable[ply][0] = move;
  }
}

void SearchWorker::updateCounterMoves(const Position &pos, Move move,
                                      Square prevSq) {
  const Square to = move.to();
  const PieceType pt = pos.getPieceType(move.from());

  counterMoveTable[~pos.getSideToMove()][pt][to] = move;
}

void SearchWorker::updateCaptureHistory(const Position &pos, SearchStack *ss,
                                        Move move, int bonus) {
  const Square to = move.to();
  const Square from = move.from();

  const PieceType captured = pos.captured(move);
  const PieceType piece = pos.movedPieceType(move);

  const bool threatFrom = pos.state()->attacked & from;
  const bool threatTo = pos.state()->attacked & to;

  captureHistoryTable[piece][threatFrom][threatTo][to][captured] << bonus;
}

Value SearchWorker::getCaptureHistory(const Position &pos, Move move) const {
  const Square to = move.to();
  const Square from = move.from();

  const PieceType captured = pos.captured(move);
  const PieceType piece = pos.movedPieceType(move);

  const bool threatFrom = pos.state()->attacked & from;
  const bool threatTo = pos.state()->attacked & to;

  return captureHistoryTable[piece][threatFrom][threatTo][to][captured];
}

Value SearchWorker::getQuietHistory(const Position &pos, Move move) const {
  const Square to = move.to();
  const Square from = move.from();

  const Colour us = pos.getSideToMove();

  const bool threatFrom = pos.state()->attacked & from;
  const bool threatTo = pos.state()->attacked & to;

  return historyTable[us][threatFrom][threatTo][from][to];
}

void SearchWorker::updateContinuationHistory(SearchStack *ss, PieceType pt,
                                             Square to, int bonus) {
  for (int i : {1, 2, 3, 4}) {
    // Only update the first two continuation if we are in check, as
    // the continuations would be easily broken in this scenario,
    // making the scores useless
    if (ss->inCheck && i > 2)
      break;
    if ((ss - i)->currentMove)
      (*(ss - i)->ch)[pt][to] << bonus;
  }
}

} // namespace Maestro
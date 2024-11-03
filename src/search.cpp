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

  for (bool isCapture : {false, true})
    for (PieceType pt = PAWN; pt <= KING; ++pt)
      for (Square to = A1; to <= H8; ++to)
        for (int cont : {0, 1})
          continuationTable[isCapture][pt][to].fill(
              StatsEntry<ContinuationHistory, NOT_USED>{});
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
  const U64 nodesUsedForBestMove = threads.getBestThread()->worker->nodes;
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

  // Allocate search stack
  // (ss - 3) is accessed from (ss - 1) when evaluating continuation histories
  SearchStack stack[MAX_PLY + 3] = {};
  SearchStack *ss = stack + 3;

  for (int i = 3; i > 0; --i) {
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
    bestValue = search<ROOT>(rootPos, ss, alpha, beta, adjustedDepth, false);

    std::stable_sort(rootMoves.begin() + pvIdx, rootMoves.begin() + pvIdx + 1);

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

  // Print PV if its the main thread, the loop is the last one or the node
  // count exceeds 10M, and the result isn't an unproven mate search
  if (isMainThread() and
      (threads.stop || pvIdx + 1 == multiPV || nodes > 10000000) and
      !(threads.abortedSearch and abs(rootMoves[0].score) <= VAL_MATE_BOUND))
    getPV(*this, rootDepth);
}

template <NodeType nodeType>
Value SearchWorker::search(Position &pos, SearchStack *ss, int depth,
                           Value alpha, Value beta, bool cutNode) {
  constexpr bool pvNode = nodeType != CUT;
  constexpr bool rootNode = nodeType == ROOT;

  // Step 1: if depth is 0, go into quiescence search
  if (depth <= 0)
    return qSearch < pvNode ? PV : CUT > (pos, ss, alpha, beta);

  // Limit depth if there is a extension explosion
  depth = std::min(depth, MAX_PLY - 1);

  // Step 3: Start Main Search (Initialise variables)
  Move pv[MAX_PLY + 1];
  BoardState st{};

  Key hashKey;
  Move move, excludedMove, bestMove;
  Depth extension, newDepth;
  Value bestValue, value, eval, maxValue;
  bool givesCheck, improving, prevIsCapture;
  bool capture, ttCapture, opponentWorsening;
  Piece movedPiece;

  Array<Move, 32> capturesSearched;
  Array<Move, 32> quietsSearched;

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
        ss->ply >= MAX_PLY)
      return (ss->ply >= MAX_PLY && !ss->inCheck) ? Eval::evaluate(pos)
                                                  : valueDraw(nodes);

    // Step 4: Mate distance pruning, if a shorter mate was found upward in the
    // tree, then there is no way to beat the alpha score. Same thing happens
    // for beta but reversed.
    alpha = std::max(mateIn(ss->ply), alpha);
    beta = std::min(matedIn(ss->ply), beta);

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
      (ttData.flag & ttData.value >= beta ? BOUND_ALPHA : BOUND_BETA)) {

    // if table move cutoff is only valid when the fifty move rule is violated
    // then there should not be a cutoff, therefore don't produce a
    // transposition table cutoff
    if (pos.getFiftyMove() < 90)
      return ttData.value;
  }

  // Step 6: Static evaluation
  eval = ss->staticEval = ss->inCheck               ? VAL_NONE
                          : ttData.eval != VAL_NONE ? ttData.eval
                                                    : Eval::evaluate(pos);

  // Set up improving flag (if the static evaluation is better than the
  // previous)
  improving = ss->staticEval > (ss - 2)->staticEval;
  // Set up the opponent Worsening flag (If the static evaluation is shifting in
  // our favour)
  opponentWorsening = ss->staticEval + (ss - 1)->staticEval > 2;

  // Step 7: Internal Iterative Deepening
  if (pvNode and !ttData.move)
    depth -= 3;

  if (depth <= 0)
    return qSearch<PV>(pos, ss, alpha, beta);

  if (cutNode and depth >= 7 and (!ttData.move or ttData.flag == BOUND_BETA))
    depth -= 3;

  return 0;
}

template <NodeType nodeType>
Value SearchWorker::qSearch(Position &pos, SearchStack *ss, Value alpha,
                            Value beta) {
  return 0;
}

} // namespace Maestro
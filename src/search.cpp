
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

constexpr int FutilityPruningHistoryLimit[] = {14296, 6004};
constexpr int ContinuationPruningDepth[] = {3, 2};
constexpr int ContinuationPruningHistoryLimit[] = {-1000, -2500};

void initLMR() {

  // Init Late Move Reductions Table
  for (int depth = 1; depth < 64; depth++)
    for (int played = 1; played < 64; played++)
      LMRTable[depth][played] = 0.7844 + log(depth) * log(played) / 2.4696;

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

  alpha = std::max(avg - delta, -VAL_INFINITE);
  beta = std::min(avg + delta, VAL_INFINITE);

  // Start with small window size, if fail high or low, increase window size
  int failedHighCnt = 0;

  // Aspiration Window loop
  while (true) {

    // Calculate adjusted depth, if fail high, reduce depth to focus on
    // other moves
    Depth adjustedDepth = std::max(1, rootDepth - failedHighCnt);
    bestValue = search<ROOT>(rootPos, ss, adjustedDepth, alpha, beta, false);

    std::stable_sort(rootMoves.begin() + pvIdx, rootMoves.end());

    // If the search was stopped, break out of the loop
    if (threads.stop)
      break;

    // Fail Low (Reduce alpha and beta for a better window)
    if (bestValue <= alpha) {
      beta = (alpha + beta) / 2;
      alpha = std::max(bestValue - delta, -VAL_INFINITE);

      failedHighCnt = 0;
      // Fail High (Increase beta for a better window)
    } else if (bestValue >= beta) {
      beta = std::min(bestValue + delta, VAL_INFINITE);
      ++failedHighCnt;
      // Exact score returned, search is complete
    } else
      break;

    // Increase window size
    delta += delta / 3;
  }

  std::stable_sort(rootMoves.begin(), rootMoves.end());

  // Print PV if its the main thread, the loop is the last one or the node
  // count exceeds 10M, and the result isn't an unproven mate search
  if (isMainThread() and
      (threads.stop || pvIdx + 1 == config.multiPV || nodes > 10000000) and
      !(threads.abortedSearch and abs(rootMoves[0].score) <= VAL_MATE_BOUND))
    getPV(*this, rootDepth);
}

template <NodeType nodeType>
Value SearchWorker::search(Position &pos, SearchStack *ss, Depth depth,
                           Value alpha, Value beta, bool cutNode) {}

template <NodeType nodeType>
Value SearchWorker::qSearch(Position &pos, SearchStack *ss, Value alpha,
                            Value beta) {}

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

Value SearchWorker::getCaptureHistory(const Position &pos, Move move) const {
  const Square to = move.to();
  const Square from = move.from();

  const PieceType captured = pos.captured(move);
  const PieceType piece = pos.movedPiece(move);

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
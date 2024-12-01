#include <iostream>

#include "eval.hpp"
#include "history.hpp"
#include "movepicker.hpp"
#include "search.hpp"
#include "uci.hpp"

namespace Maestro {

// Initialise the time manager
void TimeManager::init(Limits &limits, Colour us, int ply) {

  startTime = limits.startTime;
  // If we have no time, we don't need to continue
  if (limits.time[us] == 0)
    return;

  const TimePt time = limits.time[us];
  const TimePt inc = limits.inc[us];

  int mtg = limits.movesToGo ? std::min(limits.movesToGo, 50) : 50;

  if (limits.movesToGo > 0) {
    // X / Y + Z time management
    optimumTime = 1.50 * (time - MOVE_OVERHEAD) / mtg + inc;
    maximumTime = 3.00 * (time - MOVE_OVERHEAD) / mtg + inc;
  } else {
    // X + Y time management
    optimumTime = 1.50 * (time - MOVE_OVERHEAD + 25 * inc) / 50;
    maximumTime = 3.00 * (time - MOVE_OVERHEAD + 25 * inc) / 50;
  }

  // Cap time allocation using move overhead
  optimumTime = std::min(optimumTime, time - MOVE_OVERHEAD);
  maximumTime = std::min(maximumTime, time - MOVE_OVERHEAD);

  std::cout << "Optimum: " << optimumTime << std::endl;
  std::cout << "Maximum: " << maximumTime << std::endl;
}

// Update the total search time and check if its exceeded
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

// Check if we should stop the search
void SearchWorker::checkTime() const {
  if (isMainThread() && completedDepth >= 1 &&
      ((limits.isUsingTM() && tm.elapsed() >= tm.maximum()) ||
       (limits.movetime && tm.elapsed() >= limits.movetime)))
    threads.stop = threads.abortedSearch = true;
}

void SearchWorker::getPV(SearchWorker &best, Depth depth) const {
  auto &rootMoves = best.rootMoves;
  auto &pos = best.rootPos;

  const U64 nodes = threads.nodesSearched();

  bool updated = rootMoves[0].score != -VAL_INFINITE;

  Depth d = updated ? depth : std::max(1, depth - 1);
  Value v = updated ? rootMoves[0].score : rootMoves[0].prevScore;

  if (v == -VAL_INFINITE)
    v = VAL_ZERO;

  bool isExact = !updated;

  std::string pv;

  for (Move m : rootMoves[0].pv)
    pv += move2Str(m) + " ";

  if (!pv.empty())
    pv.pop_back();

  PrintInfo info;

  info.depth = d;
  info.score = v;
  info.selDepth = rootMoves[0].selDepth;
  info.timeMs = tm.elapsed() + 1;
  info.nodes = nodes;
  info.nps = nodes * 1000 / info.timeMs;
  info.pv = pv;
  info.hashFull = tt.hashFull();

  UCI::uciReport(info);
}

void SearchWorker::updatePV(Move *pv, Move best, const Move *childPV) const {

  for (*pv++ = best; childPV && *childPV != Move::none();)
    *pv++ = *childPV++;

  *pv = Move::none();
}

void SearchWorker::clear() {
  kt.clear();
  ht.clear();
  cht.clear();
}

void SearchWorker::startSearch() {

  // for (auto &rm : rootMoves)
  //   std::cout << move2Str(rm.pv[0]) << std::endl;

  if (!isMainThread()) {
    iterativeDeepening();
    return;
  }

  tm.init(limits, rootPos.sideToMove(), rootPos.gamePlies());

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

  // If we are using time control (no specified depth), and we have a PV, then
  // get the best threads out of the thread pool
  if (!limits.depth && rootMoves[0].pv[0] != Move::none())
    best = threads.getBestThread()->worker.get();

  // If we have a different best thread, print the PV of the best thread
  if (best != this)
    getPV(*best, best->getCompletedDepth());

  auto bestMove = move2Str(best->rootMoves[0].pv[0]);
  std::cout << "bestmove " << bestMove << std::endl;
}

void SearchWorker::iterativeDeepening() {
  Move pv[MAX_PLY + 1];

  Depth lastBestMoveDepth = 0;
  Value lastBestMoveScore = -VAL_INFINITE;
  auto lastBestPV = std::vector<Move>{Move::none()};

  Value bestValue = -VAL_INFINITE;
  Colour us = rootPos.sideToMove();
  int pvStability = 0;
  int delta = 0;

  // Allocate search stack
  SearchStack stack[MAX_PLY + 1] = {};
  SearchStack *ss = stack;

  for (int i = 0; i <= MAX_PLY; i++)
    (ss + i)->ply = i;

  ss->pv = pv;

  while (++rootDepth < MAX_PLY && !threads.stop &&
         !(limits.depth && isMainThread() && rootDepth > limits.depth)) {
    // Save previous iteration's scores
    for (RootMove &rm : rootMoves)
      rm.prevScore = rm.score;

    searchPosition(ss, bestValue);

    if (threads.stop)
      break;

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

    if (limits.isUsingTM() && !threads.stop &&
        (checkTM(lastBestMoveDepth, pvStability, bestValue) ||
         tm.elapsed() >= tm.maximum()))
      threads.stop = threads.abortedSearch = true;
  }
}

void SearchWorker::searchPosition(SearchStack *ss, Value &bestValue) {
  // Reset selDepth
  selDepth = 0;

  // aspirationWindows(ss, bestValue);
  bestValue =
      search<ROOT>(rootPos, ss, rootDepth, -VAL_INFINITE, VAL_INFINITE, false);

  std::stable_sort(rootMoves.begin(), rootMoves.end());

  if (threads.stop)
    return;

  if (isMainThread() &&
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
  Move move, bestMove;
  Value bestValue, value;
  int moveCount;

  // Initialise node
  Colour us = pos.sideToMove();
  ss->moveCount = moveCount = 0;
  ss->inCheck = pos.isInCheck();
  bestValue = -VAL_INFINITE;
  bestMove = Move::none();

  // Check for remaining time
  if (isMainThread())
    checkTime();

  // Update seldepth to sent to GUI
  if (pvNode)
    selDepth = std::max(selDepth, ss->ply + 1);

  // If we are not in the root node, check for draws and apply mate distance
  // pruning
  if (!rootNode) {
    if (pos.isDraw(ss->ply) || threads.stop.load(std::memory_order_relaxed) ||
        ss->ply >= MAX_PLY)
      return (ss->ply >= MAX_PLY && !ss->inCheck) ? Eval::evaluate(pos)
                                                  : valueDraw(nodes);

    // The worst we can do is get mated in the current ply, so the worst score
    // is mated in ply
    alpha = std::max(matedIn(ss->ply), alpha);
    // The best score is mating the opponent in the current ply, so the best
    // score is mate in ply + 1
    beta = std::min(mateIn(ss->ply + 1), beta);

    // If the score returned is better or worse than all the possible scores
    // achievable, then there is no reason to continue searching this node
    if (alpha >= beta)
      return alpha;
  }

  MovePicker mp(pos, Move::none(), depth, ss->ply, ht, kt, cht);

  while ((move = mp.next()) != Move::none()) {
    // Clear stack pvs
    if (pvNode)
      (ss + 1)->pv = nullptr;

    // Output current move to UCI
    if (rootNode && isMainThread() && tm.elapsed() > 2500) {
      UCI::uciReportCurrentMove(depth, move, moveCount);
      UCI::uciReportNodes(threads, tt.hashFull(), tm.elapsed());
    }

    ss->moveCount = moveCount++;
    U64 nodeCount = rootNode ? U64(nodes) : 0;

    // Make the move
    pos.makeMove(move, st);
    // Prefetch the next entry in the TT
    TTable::prefetch(tt.firstEntry(pos.key()));
    // Update node count
    nodes.fetch_add(1, std::memory_order_relaxed);

    // Set new PV
    (ss + 1)->pv = pv;
    (ss + 1)->pv[0] = Move::none();

    // Recursive search
    value = -search<PV>(pos, ss + 1, depth - 1, -beta, -alpha, false);

    // Unmake the move
    pos.unmakeMove(move);

    // Check for stopping conditions
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

      } else
        // All other moves but the PV are set to the lowest value
        rm.score = -VAL_INFINITE;
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
  }

  // Check for mate/stalemate
  if (!moveCount)
    bestValue = ss->inCheck ? matedIn(ss->ply) : VAL_ZERO;

  return bestValue;
}

template <NodeType nodeType>
Value SearchWorker::qSearch(Position &pos, SearchStack *ss, Value alpha,
                            Value beta) {

  // Update node count
  constexpr bool pvNode = nodeType == PV;
  // State variables
  Move pv[MAX_PLY + 1];
  BoardState st;

  Key hashKey;
  Move move, bestMove;
  Value bestValue, value;
  int moveCount;

  // Initialise node
  if (pvNode) {
    (ss + 1)->pv = pv;
    ss->pv[0] = Move::none();
  }

  // Initialise node
  Colour us = pos.sideToMove();
  ss->moveCount = moveCount = 0;
  ss->inCheck = pos.isInCheck();
  bestMove = Move::none();

  // Update seldepth to sent to GUI
  if (pvNode)
    selDepth = std::max(selDepth, ss->ply);

  // Check for draws and max ply
  if (pos.isDraw(ss->ply) || ss->ply >= MAX_PLY)
    return (ss->ply >= MAX_PLY && !ss->inCheck) ? Eval::evaluate(pos)
                                                : valueDraw(nodes);

  bestValue = Eval::evaluate(pos);

  if (bestValue >= beta)
    return bestValue;

  alpha = std::max(alpha, bestValue);

  MovePicker mp(pos, Move::none(), DEPTH_QS, ss->ply, ht, kt, cht);

  while ((move = mp.next()) != Move::none()) {

    ss->moveCount = moveCount++;

    // Make the move
    pos.makeMove(move, st);
    // Prefetch the next entry in the TT
    TTable::prefetch(tt.firstEntry(pos.key()));
    // Update node count
    nodes.fetch_add(1, std::memory_order_relaxed);
    // Recursive quiescence search
    value = -qSearch<nodeType>(pos, ss + 1, -beta, -alpha);
    // Unmake the move
    pos.unmakeMove(move);

    if (value > bestValue) {
      bestValue = value;

      if (value > alpha) {
        bestMove = move;

        if (pvNode)
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

  return bestValue;
}

} // namespace Maestro
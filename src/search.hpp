#ifndef SEARCH_HPP
#pragma once
#define SEARCH_HPP

#include <atomic>
#include <condition_variable>
#include <mutex>


#include "defs.hpp"
#include "hash.hpp"
#include "move.hpp"
#include "position.hpp"
#include "utils.hpp"

namespace Maestro {

class ThreadPool;
class TTable;

/******************************************\
|==========================================|
|              Search Limits               |
|==========================================|
\******************************************/

struct Limits {
  TimePt time[COLOUR_N], inc[COLOUR_N], movetime, startTime;
  int movesToGo, depth;
  bool perft, infinite;

  bool isUsingTM() const { return time[WHITE] || time[BLACK]; }
};

/******************************************\
|==========================================|
|               Time Manager               |
|==========================================|
\******************************************/

class TimeManager {
public:
  void init(Limits &limits, Colour us, int ply);

  TimePt optimum() const { return optimumTime; }
  TimePt maximum() const { return maximumTime; }

  TimePt elapsed() const { return getTimeMs() - startTime; }

  void clear() {
    startTime = optimumTime = maximumTime = 0;
  } // Clear time manager

private:
  TimePt startTime;
  TimePt optimumTime;
  TimePt maximumTime;
};

/******************************************\
|==========================================|
|                Root Move                 |
|==========================================|
\******************************************/

// Root Move, used to store information about the root move and its PV
struct RootMove {
  explicit RootMove(Move m) : pv(1, m) {}
  bool operator==(const Move &m) const { return pv[0] == m; }
  // Used for sorting
  bool operator<(const RootMove &m) const {
    return score != m.score ? score > m.score : prevScore > m.prevScore;
  }

  Value score = -VAL_INFINITE;
  Value prevScore = -VAL_INFINITE;
  U64 effort = 0;
  int selDepth = 0;
  std::vector<Move> pv;
};

using RootMoves = std::vector<RootMove>;

/******************************************\
|==========================================|
|              Search Stack                |
|==========================================|
\******************************************/

// Search Stack, used to store search information when searching the tree,
// remembering the information in parent nodes
struct SearchStack {
  Move *pv;
  int ply = 0;
  Move currentMove = Move::none();
  Move excludedMove = Move::none();
  Value staticEval = 0;
  int moveCount = 0;
  Depth extensions = 0;
  bool inCheck = false;
  bool ttPV = false;
  bool ttHit = false;
  int cutOffCnt = 0;
};

/******************************************\
|==========================================|
|              Search State                |
|==========================================|
\******************************************/

// Shared State, used to store information shared between threads
struct SearchState {
  SearchState(ThreadPool &threads, TTable &tt) : threads(threads), tt(tt) {}

  ThreadPool &threads;
  TTable &tt;
};

enum NodeType { PV, NON_PV, ROOT };

/******************************************\
|==========================================|
|              Search Worker               |
|==========================================|
\******************************************/

class SearchWorker {
public:
  SearchWorker(SearchState &sharedState, size_t threadId)
      : sharedState(sharedState), threadId(threadId), tt(sharedState.tt),
        threads(sharedState.threads) {}

  void clear();

  void startSearch();

  bool isMainThread() const { return threadId == 0; }

  int getCompletedDepth() const { return completedDepth; }
  int getSelDepth() const { return selDepth; }

  TimeManager tm;

private:
  void iterativeDeepening();
  void searchPosition(SearchStack *ss, Value &bestValue);

  void getPV(SearchWorker &best, Depth depth) const;
  void updatePV(Move *pv, Move best, const Move *childPv) const;

  template <NodeType nodeType>
  Value search(Position &pos, SearchStack *ss, Depth depth, Value alpha,
               Value beta, bool cutNode);

  template <NodeType nodeType>
  Value qSearch(Position &pos, SearchStack *ss, Value alpha, Value beta);

  size_t threadId;
  SearchState &sharedState;
  Limits limits;

  ThreadPool &threads;
  TTable &tt;

  size_t pvIdx, pvLast;
  int selDepth, completedDepth;
  std::atomic<U64> nodes;

  Position rootPos;
  BoardState rootState;
  RootMoves rootMoves;
  Depth rootDepth;
  Value rootDelta;

  Value bestPreviousScore, bestPreviousAvgScore;

  int failHigh, failHighFirst;

  friend class ThreadPool;
};

} // namespace Maestro

#endif // SEARCH_HPP
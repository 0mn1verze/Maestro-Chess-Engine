#ifndef SEARCH_HPP
#define SEARCH_HPP

#include <atomic>
#include <vector>

#include "defs.hpp"
#include "eval.hpp"
#include "move.hpp"
#include "movepick.hpp"
#include "position.hpp"

namespace Maestro {

// Heavily inspired by Stockfish's design, with some modifications to fit my
// engine

class ThreadPool;
class TTable;
struct Config;

/******************************************\
|==========================================|
|              Search Limits               |
|==========================================|
\******************************************/

// Search Limits, used to store information about the time constraints
struct Limits {
  TimePt time[COLOUR_N], inc[COLOUR_N], movetime, startTime;
  int movesToGo, depth, mate;
  bool perft, infinite;
  U64 nodes;
  std::vector<std::string> searchMoves;

  bool useTimeManagement() const { return time[WHITE] || time[BLACK]; }
  void trace() const;
};

/******************************************\
|==========================================|
|               Time Manager               |
|==========================================|
\******************************************/

class TimeManager {
public:
  void init(Limits &limits, Colour us, int ply, const Config &config);

  TimePt optimum() const { return optimumTime; }
  TimePt maximum() const { return maximumTime; }

  TimePt elapsed() const { return getTimeMs() - startTime; }

private:
  TimePt startTime;
  TimePt optimumTime;
  TimePt maximumTime;
};

enum NodeType { PV, CUT, ROOT };

/******************************************\
|==========================================|
|              Search Stack                |
|==========================================|
\******************************************/

// Search Stack, used to store search information when searching the tree,
// remembering the information in parent nodes
struct SearchStack {
  Move *pv;
  ContinuationHistory *ch;
  int ply = 0;
  Move currentMove = Move::none();
  Move excludedMove = Move::none();
  Value staticEval = 0;
  int moveCount = 0;
  bool inCheck = false;
  bool ttPV = false;
  bool ttHit = false;
  int cutOffCnt = 0;
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
  Value averageScore = -VAL_INFINITE;
  U64 effort = 0;
  int selDepth = 0;
  std::vector<Move> pv;
};

using RootMoves = std::vector<RootMove>;

/******************************************\
|==========================================|
|              Search State                |
|==========================================|
\******************************************/

// Shared State, used to store information shared between threads
struct SearchState {
  SearchState(Config &config, ThreadPool &threads, TTable &tt)
      : config(config), threads(threads), tt(tt) {}

  Config &config;
  ThreadPool &threads;
  TTable &tt;
};

/******************************************\
|==========================================|
|              Search Worker               |
|==========================================|
\******************************************/

class SearchWorker {
public:
  SearchWorker(SearchState &sharedState, size_t threadId)
      : sharedState(sharedState), threadId(threadId), tt(sharedState.tt),
        config(sharedState.config), threads(sharedState.threads) {}

  void clear();

  void startSearch();

  bool isMainThread() const { return threadId == 0; }

  int getCompletedDepth() const { return completedDepth; }
  int getSelDepth() const { return selDepth; }

  TimeManager tm;

  KillerTable killerTable;
  CounterMoveTable counterMoveTable;
  HistoryTable historyTable;
  CaptureHistoryTable captureHistoryTable;
  ContinuationTable continuationTable;

private:
  void iterativeDeepening();
  void aspirationWindows(SearchStack *ss, Value &alpha, Value &beta,
                         Value &delta, Value &bestValue);

  void getPV(SearchWorker &best, Depth depth) const;
  void updatePV(Move *pv, Move best, const Move *childPv) const;

  template <NodeType nodeType>
  Value search(Position &pos, SearchStack *ss, Depth depth, Value alpha,
               Value beta, bool cutNode);

  template <NodeType nodeType>
  Value qSearch(Position &pos, SearchStack *ss, Value alpha, Value beta);

  bool checkTM(Depth &, int &, int &) const;
  void checkTime() const;

  void updateAllStats(const Position &pos, SearchStack *ss, Move bestMove,
                      Square prevSq, std::vector<Move> &quietsSearched,
                      std::vector<Move> &capturesSearched, Depth depth);
  void updateQuietHistories(const Position &pos, SearchStack *ss, Move move,
                            int bonus);
  void updateCaptureHistories(const Position &pos, SearchStack *ss, Move move,
                              int bonus);
  void updateContinuationHistories(SearchStack *ss, PieceType pt, Square to,
                                   int bonus);
  void updateKillerMoves(Move move, int ply);

  void updateCounterMoves(const Position &pos, Move move, Square prevSq);

  size_t threadId;
  SearchState &sharedState;
  Limits limits;

  const Config &config;
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

  friend class ThreadPool;
};

} // namespace Maestro

#endif // SEARCH_HPP
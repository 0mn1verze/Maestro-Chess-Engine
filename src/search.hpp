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

enum NodeType { PV, CUT, ROOT };

// Search Stack, used to store search information when searching the tree,
// remembering the information in parent nodes
struct SearchStack {
  Move *pv;
  ContinuationHistory *ch;
  int ply;
  Move currentMove;
  Move excludedMove;
  Value staticEval;
  int moveCount;
  bool inCheck;
  bool ttPV;
  bool ttHit;
  int cutOffCnt;
};

struct Limits {
  TimePt time[COLOUR_N], inc[COLOUR_N], movetime, startTime;
  int movesToGo, depth;
  bool perft, infinite;
  U64 nodes;

  bool useTimeManagement() const { return time[WHITE] || time[BLACK]; }
  void trace() const;
};

// Root Move, used to store information about the root move and its PV
struct RootMove {
  explicit RootMove(Move m) : pv(1, m) {}
  bool operator==(const Move &m) const { return pv[0] == m; }
  // Used for sorting
  bool operator<(const RootMove &m) const {
    return score != m.score ? score < m.score : prevScore < m.prevScore;
  }

  Value score = -VAL_INFINITE;
  Value prevScore = -VAL_INFINITE;
  int selDepth = 0;
  std::vector<Move> pv;
};

using RootMoves = std::vector<RootMove>;

class ThreadPool;
class TTable;

// Shared State, used to store information shared between threads
struct SearchState {
  SearchState(ThreadPool &threads, TTable &tt) : threads(threads), tt(tt) {}

  ThreadPool &threads;
  TTable &tt;
};

class SearchWorker {
public:
  SearchWorker(SearchState &sharedState, size_t threadId)
      : sharedState(sharedState), threadId(threadId) {}

  void clear();

  void startSearch();

  bool isMainThread() const { return threadId == 0; }

  KillerTable killerTable;
  CounterMoveTable counterMoveTable;
  HistoryTable historyTable;
  CaptureHistoryTable captureHistoryTable;
  ContinuationTable continuationTable;

private:
  void iterativeDeepening();

  template <NodeType nodeType>
  Value search(Position &pos, SearchStack *ss, int depth, Value alpha,
               Value beta, bool cutNode);

  template <NodeType nodeType>
  Value qSearch(Position &pos, SearchStack *ss, Value alpha, Value beta);

  TimePt elapsed() const;
  bool checkTime() const;

  size_t threadId;
  SearchState &sharedState;
  Limits limits;

  size_t pvIdx, pvLast;
  int selfDepth;
  std::atomic<U64> nodes, tbHits, bestMoveChanges;

  Position rootPos;
  BoardState rootState;
  RootMoves rootMoves;
  U8 rootDepth;
  Value rootDelta;

  friend class ThreadPool;
};

} // namespace Maestro

#endif // SEARCH_HPP
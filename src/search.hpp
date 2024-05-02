#ifndef SEARCH_HPP
#define SEARCH_HPP

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <memory>

#include "bitboard.hpp"
#include "defs.hpp"
#include "movegen.hpp"
#include "position.hpp"
#include "utils.hpp"

struct PVLine {
  int length;
  Move line[MAX_DEPTH + 1];
};

struct SearchStats {
  Move killer[MAX_DEPTH][2]{};
  Value history[PIECE_N][SQ_N];
  Value captureHistory[PIECE_N][SQ_N][PIECE_TYPE_N];
};

// Time controls
struct TimeControl {
  Time startTime, idealStopTime, maxStopTime;
  Time elapsed() const { return getTimeMs() - startTime; }
};

struct NodeState {
  Value staticEval;
  Move excluded = Move::none();
  Count dExtensions = 0;
};

struct Result {
  Value best;
  PVLine pv;
};

class SearchWorker {
public:
  SearchWorker() : searchThread(&SearchWorker::idleLoop, this) {
    waitForSearchFinished();
  };
  ~SearchWorker();

  void idleLoop();
  void startSearch();
  void waitForSearchFinished();
  void start(Position &, StateList &, TimeControl &, Depth);

  void searchPosition();
  void checkTime();
  bool finishSearch();
  void tmUpdate();
  std::string pv(PVLine pvLine);

  TimeControl tm;

  std::mutex mutex;
  std::condition_variable cv;
  bool searching;
  bool quit, stop;

  SearchStats ss;

  Position rootPos;
  BoardState rootState;
  Depth maxDepth, currentDepth, selDepth;
  Count nodes;

  int pvStability = 0;
  int ply = 0;

  NodeState nodeStates[MAX_DEPTH + 1];
  Result results[MAX_DEPTH + 1];

  std::thread searchThread;

private:
  void iterativeDeepening();
  void aspirationWindow();
  void updateKiller(Move killer, int ply);
  void updateHistory(Position &pos, Move history, Depth depth);
  void updateCaptureHistory(Position &pos, Move history, Depth depth);
  Value search(Position &pos, PVLine &pv, Value alpha, Value beta, Depth depth,
               bool cutNode);
  Value quiescence(Position &pos, PVLine &parentPV, Value alpha, Value beta);
};

#endif // SEARCH_HPP
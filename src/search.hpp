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
};

// Time controls
struct TimeControl {
  Time startTime, idealStopTime, maxStopTime;
  Time elapsed() const { return getTimeMs() - startTime; }
};

class SearchWorker {
public:
  SearchWorker() : searchThread(&idleLoop, this) { waitForSearchFinished(); };
  ~SearchWorker();

  void idleLoop();
  void startSearch();
  void waitForSearchFinished();
  void start(Position &, StateList &, TimeControl &, Depth);

  void clear();
  void searchPosition();
  void resetPosition();
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
  Depth maxDepth, currentDepth;
  Count nodes, bestMoveChanges;
  Value bestValue[MAX_DEPTH + 1];
  PVLine pvLine[MAX_DEPTH + 1];
  int pvStability = 0;
  int ply = 0;

  Count failHigh, failHighFirst;

  std::thread searchThread;

private:
  void iterativeDeepening();
  void aspirationWindow();
  void updateKiller(Move killer, Depth depth);
  void updateHistory(Position &pos, Move history, Depth depth);
  Value search(Position &pos, PVLine &pv, Value alpha, Value beta, Depth depth,
               bool cutNode);
  Value quiescence(Position &pos, PVLine &parentPV, Value alpha, Value beta);
};

#endif // SEARCH_HPP
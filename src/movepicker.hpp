#ifndef MOVEPICKER_HPP
#define MOVEPICKER_HPP

#include <functional>

#include "defs.hpp"
#include "history.hpp"
#include "move.hpp"
#include "movegen.hpp"
#include "position.hpp"
#include "utils.hpp"

namespace Maestro {

enum GenStage {
  // Generate main search moves
  MAIN_TT,
  CAPTURE_INIT,
  GOOD_CAPTURE,
  QUIET_INIT,
  QUIET,
  BAD_CAPTURE,

  // Generate qsearch moves
  Q_TT,
  QCAPTURE_INIT,
  QCAPTURE,

  // Probe cut search
  PROBCUT_TT,
  PROBCUT_INIT,
  PROBCUT,
};

// Move Picker class
class MovePicker {
public:
  // Main Move Picker constructor
  MovePicker(const Position &, const Move, const Depth, const int,
             HistoryTable &, KillerTable &, CaptureHistoryTable &,
             Continuation **);

  // Probe Cut Move Picker Constructor
  MovePicker(const Position &, const Move, CaptureHistoryTable &,
             int threshold);

  // Get the next move (After move ordering)
  Move next();
  // Skip the rest of the quiet moves
  void skipQuietMoves() { _skipQuiets = true; }
  // Get the current stage
  int stage() { return _stage; }

private:
  // Score the moves
  template <GenType T> void score();

  size_t argMax();
  bool goodCaptureFilter();

  // Return best move based on move ordering score
  Move best(std::function<bool()> predicate);

  HistoryTable *_ht;
  CaptureHistoryTable *_cht;
  Continuation **_ch;

  const Position &_pos;

  int _stage;

  Move _ttMove, _killer1, _killer2;
  size_t _cur, _endBadCap, _end;

  int _threshold;
  Depth _depth;
  int _ply;
  bool _skipQuiets;
  Move _moves[MAX_MOVES];
  Value _values[MAX_MOVES];
};

} // namespace Maestro
#endif
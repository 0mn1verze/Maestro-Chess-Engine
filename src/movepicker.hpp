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
  CAPTURE_INIT,
  GOOD_CAPTURE,
  QUIET_INIT,
  QUIET,
  BAD_CAPTURE,

  // Generate qsearch moves
  QCAPTURE_INIT,
  QCAPTURE,

  // Probe cut search
  PROBCUT_INIT,
  PROBCUT,
};

// Move Picker class
class MovePicker {
  // Move Score structure used for move ordering
  struct MoveScore {
    Move move;
    Value score;

    MoveScore(const Move m) : move(m), score(0) {}
    MoveScore(const Move m, const Value s) : move(m), score(s) {}

    MoveScore() : move(Move::none()), score(0) {}

    bool operator<(const MoveScore &rhs) const { return score < rhs.score; }
    bool operator>(const MoveScore &rhs) const { return score > rhs.score; }
    bool operator==(const MoveScore &rhs) const { return score == rhs.score; }
    bool operator!=(const MoveScore &rhs) const { return score != rhs.score; }
    bool operator>=(const MoveScore &rhs) const { return score >= rhs.score; }
    bool operator<=(const MoveScore &rhs) const { return score <= rhs.score; }

    operator Move() const { return move; }
  };

public:
  // Main Move Picker constructor
  MovePicker(const Position &, const Move, const Depth, const int,
             const HistoryTable &, const KillerTable &,
             const CaptureHistoryTable &);

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

  // Return best move based on move ordering score
  Move best(std::function<bool()> predicate);

  const KillerTable *_killer;
  const HistoryTable *_history;
  const CaptureHistoryTable *_captureHistory;

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
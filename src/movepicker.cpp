#include <functional>
#include <iostream>

#include "bitboard.hpp"
#include "defs.hpp"
#include "move.hpp"
#include "movepicker.hpp"
#include "position.hpp"
#include "utils.hpp"

namespace Maestro {

// Main Move Picker constructor
MovePicker::MovePicker(const Position &pos, const Move ttMove,
                       const Depth depth, const int ply,
                       const HistoryTable &historyTable,
                       const KillerTable &killerTable,
                       const CaptureHistoryTable &captureHistoryTable)
    : _pos(pos), _ttMove(ttMove), _depth(depth), _history(&historyTable),
      _killer(&killerTable), _captureHistory(&captureHistoryTable),
      _skipQuiets(depth == DEPTH_QS), _ply(ply), _cur(0), _end(0) {
  _killer1 = _killer->probe(ply, depth);
  _killer2 = _killer->probe(ply, depth);

  if (pos.isCapture(_killer1))
    _killer1 = Move::none();
  if (pos.isCapture(_killer2))
    _killer2 = Move::none();

  _stage = (depth > DEPTH_QS ? CAPTURE_INIT : QCAPTURE_INIT);
}

template <GenType Type> void MovePicker::score() {}

size_t MovePicker::argMax() {
  return std::distance(_values,
                       std::max_element(&_values[_cur], &_values[_end]));
}

// Doing one step of insertion sort, swapping the current move with the move
// with the best score
Move MovePicker::best(std::function<bool()> predicate) {

  while (_cur < _end) {
    size_t best = argMax();

    // Insertion sort step
    std::swap(_values[best], _values[_cur]);
    std::swap(_moves[best], _moves[_cur]);

    if (_moves[_cur] != _ttMove && predicate())
      return _moves[_cur++];

    _cur++;
  }

  return Move::none();
}

Move MovePicker::next() {

  switch (_stage) {
  case PROBCUT_INIT:
  case QCAPTURE_INIT:
  case CAPTURE_INIT:
    _cur = _endBadCap = 0;
    _end = std::distance(_moves, generateMoves<CAPTURES>(_moves, _pos));
    score<CAPTURES>();
    _stage++;
    return next();
  case GOOD_CAPTURE:
    if (best([&]() { return true; }))
      return _moves[_cur - 1];
    _stage++;
    [[fallthrough]];
  case QUIET_INIT:
    if (!_skipQuiets) {
      _cur = _endBadCap;
      _end = std::distance(_moves, generateMoves<QUIETS>(_moves + _cur, _pos));
      score<QUIETS>();
    }
    _stage++;
    [[fallthrough]];
  case QUIET:
    if (!_skipQuiets && best([&]() { return true; }))
      return _moves[_cur - 1];
    _cur = 0;
    _end = _endBadCap;
    _stage++;
    [[fallthrough]];
  case BAD_CAPTURE:
    if (best([&]() { return true; }))
      return _moves[_cur - 1];
    return Move::none();
  case QCAPTURE:
    if (best([&]() { return true; }))
      return _moves[_cur - 1];
    return Move::none();
  }

  return Move::none();
}

} // namespace Maestro
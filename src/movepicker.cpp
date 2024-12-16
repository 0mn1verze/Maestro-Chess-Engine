#include <functional>
#include <iostream>

#include "bitboard.hpp"
#include "defs.hpp"
#include "eval.hpp"
#include "move.hpp"
#include "movepicker.hpp"
#include "position.hpp"
#include "utils.hpp"

namespace Maestro {

// Main Move Picker constructor
MovePicker::MovePicker(const Position &pos, const Move ttMove,
                       const Depth depth, const int ply, HistoryTable &ht,
                       KillerTable &kt, CaptureHistoryTable &cht,
                       Continuation **ch)
    : _pos(pos), _ttMove(ttMove), _depth(depth), _ht(&ht), _cht(&cht),
      _skipQuiets(depth == DEPTH_QS), _ply(ply), _cur(0), _end(0), _ch(ch) {
  _killer1 = kt.probe(ply, 0);
  _killer2 = kt.probe(ply, 1);

  if (pos.isCapture(_killer1) || !_pos.isLegal(_killer1))
    _killer1 = Move::none();
  if (pos.isCapture(_killer2) || !_pos.isLegal(_killer2))
    _killer2 = Move::none();

  _stage = (depth > DEPTH_QS ? MAIN_TT : Q_TT) +
           !(pos.isLegal(_ttMove) && (!_skipQuiets || pos.isCapture(_ttMove)));
}

// Probe Cut Move Picker Constructor
MovePicker::MovePicker(const Position &pos, const Move ttMove,
                       CaptureHistoryTable &cht, int threshold)
    : _pos(pos), _ttMove(ttMove), _cur(0), _end(0), _threshold(threshold),
      _cht(&cht), _skipQuiets(true) {
  _stage = PROBCUT_TT + !(pos.isLegal(_ttMove) && pos.isCapture(_ttMove));
}

template <GenType Type> void MovePicker::score() {
  for (int i = _cur; i < _end; i++) {
    Move m = _moves[i];

    if constexpr (Type == CAPTURES) {
      _values[i] = 7 * PieceValue[_pos.capturedPiece(m)] + _cht->probe(_pos, m);
    }

    if constexpr (Type == QUIETS) {
      _values[i] = _ht->probe(_pos, m);
      _values[i] += _ch[0]->probe(_pos, m);
      _values[i] += _ch[1]->probe(_pos, m);
      _values[i] += _ch[2]->probe(_pos, m);
      _values[i] += _ch[3]->probe(_pos, m);
    }
  }
}

size_t MovePicker::bestIndex() {
  return std::distance(_values,
                       std::max_element(&_values[_cur], &_values[_end]));
}

// Doing one step of insertion sort, swapping the current move with the move
// with the best score
Move MovePicker::best(std::function<bool()> predicate) {

  for (; _cur < _end; ++_cur) {
    size_t best = bestIndex();
    // Insertion sort step
    std::swap(_values[best], _values[_cur]);
    std::swap(_moves[best], _moves[_cur]);

    if (_moves[_cur] != _ttMove && predicate())
      return _moves[_cur++];
  }

  return Move::none();
}

bool MovePicker::goodCaptureFilter() {
  if (!_pos.SEE(_moves[_cur], -_values[_cur] / 20)) {
    _moves[_endBadCap] = _moves[_cur];
    _values[_endBadCap++] = _values[_cur];
    return false;
  }
  return true;
}

bool MovePicker::quietFilter() {
  return _moves[_cur] != _killer1 && _moves[_cur] != _killer2;
}

Move MovePicker::next() {

  switch (_stage) {
  case PROBCUT_TT:
  case Q_TT:
  case MAIN_TT:
    _stage++;
    return _ttMove;
  // Capture generation stage
  case PROBCUT_INIT:
  case QCAPTURE_INIT:
  case CAPTURE_INIT:
    _cur = _endBadCap = 0;
    _end = std::distance(_moves, generateMoves<CAPTURES>(_moves, _pos));
    score<CAPTURES>();
    _stage++;
    return next();
  // Good capture stage
  case GOOD_CAPTURE:
    if (best([&]() { return goodCaptureFilter(); }))
      return _moves[_cur - 1];
    _stage++;
    [[fallthrough]];
  // Killer move stage (Killer 1)
  case KILLER1:
    _stage++;
    if (!_skipQuiets && _killer1 && _killer1 != _ttMove)
      return _killer1;
    [[fallthrough]];
  // Killer move stage (Killer 2)
  case KILLER2:
    _stage++;
    if (!_skipQuiets && _killer2 && _killer2 != _ttMove)
      return _killer2;
    [[fallthrough]];
  // Quiet generation stage
  case QUIET_INIT:
    if (!_skipQuiets) {
      _cur = _endBadCap;
      _end = _beginBadQuiets = _endBadQuiets =
          std::distance(_moves, generateMoves<QUIETS>(_moves + _cur, _pos));
      score<QUIETS>();
    }
    _stage++;
    [[fallthrough]];
  // Quiet move stage
  case GOOD_QUIET:
    if (!_skipQuiets && best([&]() { return quietFilter(); })) {

      if ((_values[_cur - 1] > -2000))
        return _moves[_cur - 1];

      _beginBadQuiets = _cur - 1;
    }
    _cur = 0;
    _end = _endBadCap;
    _stage++;
    [[fallthrough]];
  // Bad capture stage
  case BAD_CAPTURE:
    if (best([&]() { return true; }))
      return _moves[_cur - 1];
    _cur = _beginBadQuiets;
    _end = _endBadQuiets;
    _stage++;
    [[fallthrough]];
  // Bad quiet stage
  case BAD_QUIET:
    if (!_skipQuiets && best([&]() { return quietFilter(); }))
      return _moves[_cur - 1];
    return Move::none();
  // Quiescence capture stage
  case QCAPTURE:
    if (best([&]() { return true; }))
      return _moves[_cur - 1];
    return Move::none();
  // Probe cut stage
  case PROBCUT:
    if (best([&]() { return !_pos.SEE(_moves[_cur], _threshold); }))
      return _moves[_cur - 1];
    return Move::none();
  }

  return Move::none();
}

} // namespace Maestro
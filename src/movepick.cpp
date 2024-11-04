#include <iostream>

#include "bitboard.hpp"
#include "defs.hpp"
#include "move.hpp"
#include "movepick.hpp"
#include "position.hpp"
#include "utils.hpp"

namespace Maestro {

enum GenStage {
  // Generate main search moves
  MAIN_TT,
  CAPTURE_INIT,
  GOOD_CAPTURE,
  KILLER1,
  KILLER2,
  COUNTER_MOVE,
  QUIET_INIT,
  GOOD_QUIET,
  BAD_CAPTURE,
  BAD_QUIET,

  // Generate qsearch moves
  QSEARCH_TT,
  QCAPTURE_INIT,
  QCAPTURE,

  // Probe cut search
  PROBCUT_TT,
  PROBCUT_INIT,
  PROBCUT,
};

// Find the best move index in the list [start to end]
GenMove MovePicker::bestMove() {

  while (cur < endMoves) {
    std::swap(*cur, *std::max_element(cur, endMoves));

    if (!isSpecial(cur))
      return *cur++;

    cur++;
  }

  return GenMove::none();
}

MovePicker::MovePicker(const Position &pos, GenMove ttm, Depth depth,
                       const KillerTable *kt, const CounterMoveTable *cmt,
                       const HistoryTable *ht, const CaptureHistoryTable *cht,
                       const ContinuationHistory **ch, int ply)
    : kt(kt), cmt(cmt), ht(ht), cht(cht), ch(ch), pos(pos), ttMove(ttm),
      cur(moves), endMoves(moves), depth(depth), ply(ply),
      skipQuiets(depth == 0) {

  Move prevMove = ply > 0 ? pos.state()->move : Move::none();
  counterMove =
      (*cmt)[~pos.getSideToMove()][pos.getPiece(prevMove.to())][prevMove.to()];
  killer1 = (*kt)[ply][0];
  killer2 = (*kt)[ply][1];

  if (pos.isCapture(counterMove))
    counterMove = GenMove::none();
  if (pos.isCapture(killer1))
    killer1 = GenMove::none();
  if (pos.isCapture(killer2))
    killer2 = GenMove::none();

  stage = (depth > 0 ? MAIN_TT : QSEARCH_TT) +
          !(ttm.move and pos.isPseudoLegal(ttm));
}

MovePicker::MovePicker(const Position &pos, GenMove ttm, int threshold,
                       const CaptureHistoryTable *cht)
    : pos(pos), ttMove(ttm), cur(moves), endMoves(moves), threshold(threshold),
      cht(cht), skipQuiets(true) {
  stage = PROBCUT_TT +
          !(ttm.move and pos.isCapture(ttm) and pos.isPseudoLegal(ttm));
}

// Assigns numerical value to each move in a list [start to end]
template <GenType Type> void MovePicker::score() {

  constexpr int MVVAugment[] = {0, 2400, 2400, 4800, 9600};

  for (auto &m : *this) {

    const Square to = m.move.to();
    const Square from = m.move.from();

    const PieceType captured = pos.captured(m);
    const PieceType piece = pos.movedPiece(m);

    const bool threatFrom = pos.state()->attacked & from;
    const bool threatTo = pos.state()->attacked & to;

    if constexpr (Type == CAPTURES)
      m.score = 64000 + 64000 * (m.move.promoted() == QUEEN) +
                (*cht)[piece][threatFrom][threatTo][to][captured] +
                MVVAugment[captured];
    else if constexpr (Type == QUIETS) {
      m.score = (*ht)[pos.getSideToMove()][threatFrom][threatTo][from][to];
      m.score += (*ch[0])[piece][to];
      m.score += (*ch[1])[piece][to];
      m.score += (*ch[2])[piece][to];
      m.score += (*ch[3])[piece][to];
    }
  }
}

Move MovePicker::selectNext() {

  switch (stage) {
  case PROBCUT_TT:
  case QSEARCH_TT:
  case MAIN_TT:
    stage++;
    if ((pos.isCapture(ttMove) || !skipQuiets) and pos.isPseudoLegal(ttMove))
      return ttMove;
  case PROBCUT_INIT:
  case QCAPTURE_INIT:
  case CAPTURE_INIT:
    cur = endCaptures = moves;
    startQuiet = endMoves = generateMoves<CAPTURES>(cur, pos);
    score<CAPTURES>();
    stage++;
    return selectNext();
  case GOOD_CAPTURE:
    if (bestMove()) {
      if (!pos.SEE(*(cur - 1), threshold)) {
        (cur - 1)->score = -1;
        *endCaptures++ = *(cur - 1);
        selectNext();
      }
      return *(cur - 1);
    }
    stage++;
    [[fallthrough]];
  case KILLER1:
    stage++;
    if (!skipQuiets and killer1.move != ttMove and pos.isPseudoLegal(killer1))
      return killer1;
    [[fallthrough]];
  case KILLER2:
    stage++;
    if (!skipQuiets and killer2.move != ttMove and pos.isPseudoLegal(killer2))
      return killer2;
    [[fallthrough]];
  case COUNTER_MOVE:
    stage++;
    if (!skipQuiets and counterMove.move != ttMove and
        counterMove.move != killer1 and counterMove.move != killer2 and
        pos.isPseudoLegal(counterMove))
      return counterMove;
    [[fallthrough]];
  case QUIET_INIT:
    if (!skipQuiets) {
      cur = endCaptures;
      endMoves = startQuiet = endQuiet = generateMoves<QUIETS>(cur, pos);
      score<QUIETS>();
    }
    stage++;
    [[fallthrough]];
  case GOOD_QUIET:
    if (!skipQuiets and bestMove()) {
      if ((cur - 1)->score > -8000 || (cur - 1)->score <= -3560 * depth)
        return *(cur - 1);
      startQuiet = cur - 1;
    }
    cur = moves;
    endMoves = endCaptures;
    stage++;
    [[fallthrough]];
  case BAD_CAPTURE:
    if (bestMove())
      return *(cur - 1);
    stage++;
    cur = startQuiet;
    endMoves = endQuiet;
    [[fallthrough]];
  case BAD_QUIET:
    if (!skipQuiets)
      return bestMove();
    break;
  case PROBCUT:
    while (cur < endMoves) {
      if (pos.SEE(*cur, threshold))
        return *cur++;
      cur++;
    }
    break;
  case QCAPTURE:
    return bestMove();
    break;
  }

  return Move::none();
}

} // namespace Maestro

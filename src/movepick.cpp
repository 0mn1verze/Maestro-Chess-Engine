#include <iostream>

#include <functional>

#include "bitboard.hpp"
#include "defs.hpp"
#include "move.hpp"
#include "movepick.hpp"
#include "position.hpp"
#include "search.hpp"
#include "utils.hpp"

namespace Maestro {

// Find the best move index in the list [start to end]
GenMove MovePicker::bestMove(std::function<bool()> predicate) {

  while (cur < endMoves) {
    std::swap(*cur, *std::max_element(cur, endMoves));

    if (cur->move != ttMove && predicate())
      return *cur++;

    cur++;
  }

  return GenMove::none();
}

bool MovePicker::contains(Move move) {
  GenMove *m = std::find(begin(), end(), GenMove(move));
  return m != end();
}

MovePicker::MovePicker(const SearchWorker &sw, const Position &pos,
                       SearchStack *ss, Move ttm,
                       const ContinuationHistory **ch, Depth depth)
    : kt(&sw.killerTable), cmt(&sw.counterMoveTable), ht(&sw.historyTable),
      cht(&sw.captureHistoryTable), ch(ch), pos(pos), ttMove(ttm), cur(moves),
      endMoves(moves), depth(depth), ply(ss->ply),
      skipQuiets(depth == DEPTH_QS) {
  Move prevMove = ply > 0 ? (ss - 1)->currentMove : Move::none();

  counterMove = (*cmt)[~pos.getSideToMove()][pos.getPiece(prevMove.from())]
                      [prevMove.to()];

  killer1 = (*kt)[ply][0];
  killer2 = (*kt)[ply][1];

  if (pos.isCapture(counterMove))
    counterMove = Move::none();
  if (pos.isCapture(killer1))
    killer1 = Move::none();
  if (pos.isCapture(killer2))
    killer2 = Move::none();

  stage = (depth > 0 ? CAPTURE_INIT : QCAPTURE_INIT);
}

MovePicker::MovePicker(const Position &pos, Move ttm,
                       const CaptureHistoryTable *cht, int threshold)
    : pos(pos), ttMove(ttm), cur(moves), endMoves(moves), threshold(threshold),
      cht(cht), skipQuiets(true) {
  stage = PROBCUT_INIT;
}

// Assigns numerical value to each move in a list [start to end]
template <GenType Type> void MovePicker::score() {

  constexpr int MVVAugment[] = {0, 2400, 2400, 4800, 9600};

  for (auto &m : *this) {

    const Square to = m.move.to();
    const Square from = m.move.from();

    const PieceType captured = pos.captured(m);
    const PieceType piece = pos.movedPieceType(m);

    const bool threatFrom = pos.state()->attacked & from;
    const bool threatTo = pos.state()->attacked & to;

    if constexpr (Type == CAPTURES)

      if (m.move == ttMove)
        m.score = 1000000;
      else
        m.score = 64000 + 64000 * (m.move.promoted() == QUEEN) +
                  (*cht)[piece][threatFrom][threatTo][to][captured] +
                  MVVAugment[captured];
    else if constexpr (Type == QUIETS) {

      if (m.move == ttMove)
        m.score = 1000000;
      else if (m.move == counterMove)
        m.score = 32000;
      else if (m.move == killer1)
        m.score = 64000;
      else if (m.move == killer2)
        m.score = 48000;
      else {
        m.score = (*ht)[pos.getSideToMove()][threatFrom][threatTo][from][to];
        m.score += (*ch[0])[piece][to];
        m.score += (*ch[1])[piece][to];
        m.score += (*ch[2])[piece][to];
        m.score += (*ch[3])[piece][to];
      }
    }
  }
}

Move MovePicker::selectNext() {

  switch (stage) {
  case PROBCUT_INIT:
  case QCAPTURE_INIT:
  case CAPTURE_INIT:
    cur = endCaptures = moves;
    startQuiet = endMoves = generateMoves<CAPTURES>(cur, pos);
    score<CAPTURES>();
    stage++;
    return selectNext();
  case GOOD_CAPTURE:
    if (bestMove([&]() {
          return pos.SEE(*cur, -cur->score / 20)
                     ? true
                     : (*endCaptures++ = *cur, false);
        })) {
      return *(cur - 1);
    }
    stage++;
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
    if (!skipQuiets && bestMove([&] { return true; }))
      return *(cur - 1);
    cur = moves;
    endMoves = endCaptures;
    stage++;
    [[fallthrough]];
  case BAD_CAPTURE:
    if (bestMove([&] { return true; }))
      return *(cur - 1);
    return Move::none();
  case PROBCUT:
    if (bestMove([&] { return !pos.SEE(*(cur - 1), threshold); })) {
      return *(cur - 1);
    }
    return Move::none();
  case QCAPTURE:
    if (bestMove([&] { return true; }))
      return *(cur - 1);
    return Move::none();
  }

  return Move::none();
}

} // namespace Maestro

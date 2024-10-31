
#include "movepick.hpp"
#include "bitboard.hpp"
#include "defs.hpp"
#include "move.hpp"
#include "position.hpp"
#include "utils.hpp"

namespace {

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

} // namespace

// Find the best move index in the list [start to end]
GenMove *MovePicker::bestMove(GenMove *start, GenMove *end) {
  //   return std::max_element(start, end);

  while (start < end) {
    std::swap(*start, *std::max_element(start, endMoves));

    if (*start != ttMove and *start != counterMove and *start != killer1 and
        *start != killer2)
      return start++;

    start++;
  }

  return endMoves;
}

MovePicker::MovePicker(const Position &pos, GenMove ttm, U8 depth,
                       const KillerTable *kt, const CounterMoveTable *cmt,
                       const HistoryTable *ht, const CaptureHistoryTable *cht,
                       const ContinuationTable *ct, int ply)
    : kt(kt), cmt(cmt), ht(ht), cht(cht), ct(ct), pos(pos), ttMove(ttm),
      cur(moves), endMoves(moves), depth(depth), ply(ply), skipQuiets(false) {

  counterMove =
      (*cmt)[~pos.getSideToMove()][pos.movedPiece(ttm)][ttm.move.to()];
  killer1 = (*kt)[ply][0];
  killer2 = (*kt)[ply][1];

  stage = (depth > 0 ? MAIN_TT : QSEARCH_TT) +
          !(ttm.move.isValid() and pos.isLegal(ttm));
}

MovePicker::MovePicker(const Position &pos, GenMove ttm, int threshold,
                       const CaptureHistoryTable *cht)
    : pos(pos), ttMove(ttm), cur(moves), endMoves(moves), threshold(threshold),
      cht(cht), skipQuiets(true) {
  stage = PROBCUT_TT +
          !(ttm.move.isValid() and pos.isCapture(ttm) and pos.isLegal(ttm));
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
    }
  }
}

Move MovePicker::selectNext(bool skipQuiets = false) {
  GenMove *best;

  switch (stage) {
  case PROBCUT_TT:
  case QSEARCH_TT:
  case MAIN_TT:
    stage++;
    return ttMove;
  case PROBCUT_INIT:
  case QCAPTURE_INIT:
  case CAPTURE_INIT:
    cur = endCaptures = moves;
    startQuiet = endMoves = generateMoves<CAPTURES>(cur, pos);
    score<CAPTURES>();
    stage++;
    [[fallthrough]];
  case GOOD_CAPTURE:
    while (cur < endMoves) {
      best = bestMove(cur, endMoves);

      if (best->score < 0)
        break;

      if (!pos.SEE(*best, threshold)) {
        best->score = -1;
        endCaptures++;
        continue;
      }

      if (best->move == ttMove)
        continue;
      if (best->move == counterMove)
        counterMove = Move::none();
      if (best->move == killer1)
        killer1 = Move::none();
      if (best->move == killer2)
        killer2 = Move::none();

      return *best;
    }
    stage++;
    [[fallthrough]];
  case KILLER1:
    stage++;
    if (!skipQuiets and killer1.move != ttMove and pos.isLegal(killer1))
      return killer1;
    [[fallthrough]];
  case KILLER2:
    stage++;
    if (!skipQuiets and killer2.move != ttMove and pos.isLegal(killer2))
      return killer2;
    [[fallthrough]];
  case COUNTER_MOVE:
    stage++;
    if (!skipQuiets and counterMove.move != ttMove and
        counterMove.move != killer1 and counterMove.move != killer2 and
        pos.isLegal(counterMove))
      return counterMove;
    [[fallthrough]];
  case QUIET_INIT:
    if (!skipQuiets) {
      cur = endQuiet = endMoves;
      endMoves = generateMoves<QUIETS>(cur, pos);
      score<QUIETS>();
      stage = GOOD_QUIET;
    }
    [[fallthrough]];
  case GOOD_QUIET:
    if (!skipQuiets) {
      while (cur < endMoves) {
        best = bestMove(cur, endMoves);

        if (best->score > -8000 || best->score <= -3560 * depth)
          return *best;

        if (best->move == ttMove || best->move == counterMove ||
            best->move == killer1 || best->move == killer2)
          continue;

        endQuiet++;
        return *best;
      }
    }
    stage = BAD_CAPTURE;
    [[fallthrough]];
  case BAD_CAPTURE:
    cur = endCaptures;
    while (cur < startQuiet) {
      if (best->move == ttMove || best->move == counterMove ||
          best->move == killer1 || best->move == killer2)
        continue;

      return *cur++;
    }
    stage++;
    [[fallthrough]];
  case BAD_QUIET:
    cur = endQuiet;
    while (cur < endMoves) {
      if (best->move == ttMove || best->move == counterMove ||
          best->move == killer1 || best->move == killer2)
        continue;

      return *cur++;
    }
    break;
  case PROBCUT:
    while (cur < endMoves) {
      if (pos.SEE(*cur, threshold))
        return *cur++;
      cur++;
    }
    break;
  case QCAPTURE:
    return *cur++;
    break;
  }

  return Move::none();
}

#include <iostream>

#include "defs.hpp"
#include "movegen.hpp"
#include "movepicker.hpp"
#include "position.hpp"
#include "search.hpp"

// Partial insertion sort
inline void insertion_sort(Move *begin, Move *end, int limit) {
  for (Move *sortedEnd = begin, *p = begin + 1; p < end; ++p)
    if (p->score >= limit) {
      Move tmp = *p, *q;
      *p = *++sortedEnd;
      for (q = sortedEnd; q != begin && *(q - 1) < tmp; --q)
        *q = *(q - 1);
      *q = tmp;
    }
}

MovePicker::MovePicker(Position &pos, SearchStats &ss, const int ply,
                       const Depth depth, const Move pvMove,
                       const bool onlyCaptures, const int threshold)
    : pos(pos), ply(ply), cur(moves), ss(ss), depth(depth), ttMove(pvMove),
      threshold(threshold), onlyCaptures(onlyCaptures) {
  // Set the generation stage to INIT_CAPTURES
  if (onlyCaptures)
    genStage =
        GenStage(Q_TT + !(pos.isLegal(ttMove) and pos.isCapture(ttMove)));
  else
    genStage = GenStage(TT + !pos.isLegal(ttMove));

  if (!onlyCaptures) {
    killer1 = ss.killer[ply][0];
    killer2 = ss.killer[ply][1];
  } else {
    killer1 = killer2 = Move::none();
  }
};

MovePicker::MovePicker(Position &pos, SearchStats &ss, const int threshold,
                       const Move pvMove)
    : pos(pos), ply(0), cur(moves), ss(ss), depth(0), ttMove(pvMove),
      threshold(threshold), onlyCaptures(true) {

  killer1 = killer2 = Move::none();

  genStage =
      GenStage(PROBCUT_TT + !(pos.isLegal(ttMove) and pos.isCapture(ttMove) and
                              pos.SEE(ttMove, threshold)));
}

template <GenType gt> void MovePicker::scoreMoves() {
  // Loop through all the moves
  for (auto &m : *this) {
    // From and to squares
    Square from = m.from();
    Square to = m.to();
    // If move is capture, this sort using MVV_LVA
    if constexpr (gt == CAPTURES) {
      // Attacker and victim

      const PieceType captured = m.isNormal() ? pos.getPieceType(m.to()) : PAWN;
      const PieceType piece = pos.getPieceType(m.from());

      m.score = 7 * int(PieceValue[captured]) +
                ss.captureHistory[piece][to][captured];
    }

    if constexpr (gt == QUIETS) {
      // if (m == ss.killer[depth][0])
      //   m.score = 9000;
      // else if (m == ss.killer[depth][1])
      //   m.score = 8000;
      // else {
      Piece piece = pos.getPiece(from);
      m.score = ss.history[piece][to];
      // }
    }
  }
}

template <MovePicker::PickType T, typename Pred>
Move MovePicker::select(Pred filter) {
  while (cur < endMoves) {
    if constexpr (T == Best)
      std::swap(*cur, *std::max_element(cur, endMoves));

    if (*cur != ttMove and filter())
      return *cur++;

    cur++;
  }

  return Move::none();
}

Move MovePicker::pickNextMove(bool quiescence) {

  auto quietThreshold = [](Depth d) { return -200 * d; };

  switch (genStage) {
  case TT:
  case Q_TT:
  case PROBCUT_TT:
    // Increment stage and return pv move
    ++genStage;
    return ttMove;
  case INIT_CAPTURE:
  case Q_INIT_CAPTURE:
  case PROBCUT_INIT:
    // Set cur pointer
    cur = endBadCaptures = moves;
    // Generate moves
    endMoves = generateMoves<CAPTURES>(cur, pos);
    // Score moves
    scoreMoves<CAPTURES>();
    // Sort moves
    insertion_sort(cur, endMoves, std::numeric_limits<int>::min());
    // Increment stage
    ++genStage;
    return pickNextMove(quiescence);
  case GOOD_CAPTURE:
    if (select<Next>([&]() {
          return pos.SEE(*cur, -threshold) ? true
                                           : (*endBadCaptures++ = *cur, false);
        }))
      return *(cur - 1);
    // Increment stage
    ++genStage;
  case KILLER1:
    ++genStage;
    if (!onlyCaptures and killer1 != ttMove and pos.isLegal(killer1) and
        !pos.isCapture(killer1))
      return killer1;
  case KILLER2:
    ++genStage;
    if (!onlyCaptures and killer2 != ttMove and pos.isLegal(killer2) and
        !pos.isCapture(killer2))
      return killer2;
  case INIT_QUIET:
    if (!quiescence) {
      // Set cur pointer
      cur = endBadCaptures;
      // Generate moves
      endMoves = beginBadQuiets = endBadQuiets =
          generateMoves<QUIETS>(cur, pos);
      // Score moves
      scoreMoves<QUIETS>();
      // Sort moves
      insertion_sort(cur, endMoves, quietThreshold(depth));
    }
    ++genStage;
  case GOOD_QUIET:
    if (!quiescence and
        select<Next>([&]() { return *cur != killer1 and *cur != killer2; })) {
      if ((cur - 1)->score > -500 || (cur - 1)->score <= quietThreshold(depth))
        return *(cur - 1);

      beginBadQuiets = cur - 1;
    }
    cur = moves;
    endMoves = endBadCaptures;
    ++genStage;
  case BAD_CAPTURE:
    if (select<Next>([&]() { return true; }))
      return *(cur - 1);

    cur = beginBadQuiets;
    endMoves = endBadQuiets;

    ++genStage;
  case BAD_QUIET:
    if (!quiescence)
      return select<Next>(
          [&]() { return *cur != killer1 and *cur != killer2; });
    return Move::none();
  case Q_CAPTURE:
    if (select<Next>([]() { return true; }))
      return *(cur - 1);
    ++genStage;
    return Move::none();
  case PROBCUT:
    return select<Next>([&]() { return pos.SEE(*cur, threshold); });
  }
  return Move::none();
}

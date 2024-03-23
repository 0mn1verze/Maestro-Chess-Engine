#include <iostream>

#include "defs.hpp"
#include "movegen.hpp"
#include "movepicker.hpp"
#include "position.hpp"
#include "search.hpp"

MovePicker::MovePicker(Position &pos, SearchStats &ss, const int ply,
                       const Depth depth, const Move pvMove)
    : pos(pos), ply(ply), cur(moves), back(cur), ss(ss), depth(depth),
      ttMove(pvMove) {
  // Set the generation stage to INIT_CAPTURES
  genStage = PV;

  // If PV move is none, set stage to INIT_CAPTURES
  if (!pos.isLegal(ttMove))
    ++genStage;
};

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

      const bool evade = pos.state()->attacked & m.from();
      const bool threat = pos.state()->attacked & m.to();

      // MVV_LVA (Victims in ascending order and attackers in descending
      // order)
      m.score = int(captured) * 100 + PIECE_TYPE_N - int(piece) + 70000;
    }

    if constexpr (gt == QUIETS) {
      if (m == ss.killer[depth][0])
        m.score = 9000;
      else if (m == ss.killer[depth][1])
        m.score = 8000;
      else {
        Piece piece = pos.getPiece(from);
        m.score = ss.history[piece][to];
      }
    }
  }
}

Move MovePicker::pickNextMove(bool quiescence) {
  switch (genStage) {
  case PV:
    // Increment stage and return pv move
    ++genStage;
    return ttMove;
  case INIT_CAPTURES:
    // Set cur pointer
    cur = moves;
    // Generate moves
    back = generateMoves<CAPTURES>(cur, pos);
    // Score moves
    scoreMoves<CAPTURES>();
    // Sort moves
    insertion_sort(cur, back, 0);
    // Increment stage
    ++genStage;
  case G_CAPTURES:
    while (cur < back) {
      if (*cur != ttMove)
        return *cur++;
      cur++;
    }
    // Increment stage
    ++genStage;
  case INIT_QUIETS:
    if (!quiescence) {
      // Set cur pointer
      cur = back;
      // Generate moves
      back = generateMoves<QUIETS>(cur, pos);
      // Score moves
      scoreMoves<QUIETS>();
      // Sort moves
      insertion_sort(cur, back, 0);
    }
    ++genStage;
  case G_QUIETS:
    if (!quiescence) {
      // Return quiet moves if there are still quiet moves left
      while (cur < back) {
        if (*cur != ttMove)
          return *cur++;
        cur++;
      }
    }
    // Increment stage
    ++genStage;
  }
  return Move::none();
}

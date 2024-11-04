#include <iostream>

#include "bitboard.hpp"
#include "defs.hpp"
#include "eval.hpp"
#include "position.hpp"

namespace Maestro::Eval {

Array<Score, PIECE_N, SQ_N> psqt;

// Init piece square table
void initEval() {
  for (PieceType pt = PAWN; pt <= KING; ++pt) {
    for (Square sq = A1; sq <= H8; ++sq) {
      psqt[toPiece(WHITE, pt)][sq] = bonus[pt][sq] + pieceBonus[pt];
      psqt[toPiece(BLACK, pt)][flipRank(sq)] = -psqt[toPiece(WHITE, pt)][sq];
    }
  }
}

Value evaluate(const Position &pos) {
  int gamePhase = std::min(24, pos.gamePhase());
  Score score = pos.psq();

  return (score.first * gamePhase + score.second * (24 - gamePhase)) / 24;
}
} // namespace Maestro::Eval
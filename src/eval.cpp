#include <algorithm>
#include <iostream>
#include <sstream>

#include "defs.hpp"
#include "eval.hpp"
#include "position.hpp"
#include "utils.hpp"

namespace Maestro::Eval {

Score psqt[PIECE_N][SQ_N];

// Init piece square table
void initEval() {
  for (PieceType pt = PAWN; pt <= KING; ++pt) {
    for (Square sq = A1; sq <= H8; ++sq) {
      psqt[toPiece(WHITE, pt)][sq] = bonus[pt][sq] + pieceBonus[pt];
      psqt[toPiece(BLACK, pt)][flipRank(sq)] = -psqt[toPiece(WHITE, pt)][sq];
    }
  }
}

// Evaluate the position
Value evaluate(const Position &pos) {
  BoardState *st = pos.state();
  Score psq = pos.psq();

  int mgPhase = pos.gamePhase();
  if (mgPhase > 24)
    mgPhase = 24;
  int egPhase = 24 - mgPhase;

  Value classical = (psq.first * mgPhase + psq.second * egPhase) / 24;

  return pos.getSideToMove() == WHITE ? classical : -classical;
}

} // namespace Maestro::Eval
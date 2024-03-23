#include <format>
#include <iostream>
#include <sstream>

#include "defs.hpp"
#include "eval.hpp"
#include "nnue.hpp"
#include "position.hpp"
#include "utils.hpp"

// convert BBC piece code to Stockfish piece codes
int nnuePieces[PIECE_N] = {blank, wpawn,  wknight, wbishop, wrook,   wqueen,
                           wking, blank,  blank,   bpawn,   bknight, bbishop,
                           brook, bqueen, bking,   blank};

inline int evaluate_nnue(const Position &pos) {
  Bitboard bitboard;
  Square square;
  int pieces[33];
  int squares[33];
  int index = 2;

  // Set king squares
  pieces[0] = nnuePieces[wK];
  squares[0] = pos.square<KING>(WHITE);

  pieces[1] = nnuePieces[bK];
  squares[1] = pos.square<KING>(BLACK);

  for (PieceType pt = PAWN; pt <= QUEEN; ++pt) {
    bitboard = pos.getPiecesBB(WHITE, pt);
    while (bitboard) {
      square = popLSB(bitboard);
      pieces[index] = nnuePieces[toPiece(WHITE, pt)];
      squares[index] = square;
      index++;
    }

    bitboard = pos.getPiecesBB(BLACK, pt);
    while (bitboard) {
      square = popLSB(bitboard);
      pieces[index] = nnuePieces[toPiece(BLACK, pt)];
      squares[index] = square;
      index++;
    }
  }

  pieces[index] = 0;
  squares[index] = 0;

  return nnue_evaluate(pos.getSideToMove(), pieces, squares);
}

// Evaluate the position
Value eval(const Position &pos) {
  BoardState *st = pos.state();
  Value mat =
      pos.getNonPawnMaterial() + PawnValue * countBits(pos.getPiecesBB(PAWN));
  // Evaluate the position using the NNUE
  Value evaluation = evaluate_nnue(pos) * 5 / 4 + 28;
  evaluation = std::clamp(evaluation, -VAL_MATE_BOUND, VAL_MATE_BOUND);
  return evaluation * (100 - st->fiftyMove) / 100;
}
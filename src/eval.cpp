#include <algorithm>
#include <iostream>
#include <sstream>

#include "defs.hpp"
#include "eval.hpp"
#include "nnue.hpp"
#include "position.hpp"
#include "uci.hpp"
#include "utils.hpp"

namespace Maestro::Eval {

Score psqt[PIECE_N][SQ_N];

// convert BBC piece code to Stockfish piece codes
int nnuePieces[PIECE_N] = {blank, wpawn,  wknight, wbishop, wrook,   wqueen,
                           wking, blank,  blank,   bpawn,   bknight, bbishop,
                           brook, bqueen, bking,   blank};

int toNNUEPiece(Piece piece) { return nnuePieces[piece]; }

// Init piece square table
void initEval() {
  for (PieceType pt = PAWN; pt <= KING; ++pt) {
    for (Square sq = A1; sq <= H8; ++sq) {
      psqt[toPiece(WHITE, pt)][sq] = bonus[pt][sq] + pieceBonus[pt];
      psqt[toPiece(BLACK, pt)][flipRank(sq)] = -psqt[toPiece(WHITE, pt)][sq];
    }
  }
}

inline Value evaluate_nnue(const Position &pos) {
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

  if (pos.state()->plies > 2) {
    // Get previous nnue accumulator data
    NNUEdata *data[3];
    data[0] = &(pos.state()->nnueData);
    data[1] = &(pos.state()->previous->nnueData);
    data[2] = &(pos.state()->previous->previous->nnueData);

    return nnue_evaluate_incremental(pos.getSideToMove(), pieces, squares,
                                     data);
  } else
    return nnue_evaluate(pos.getSideToMove(), pieces, squares);
}

// Evaluate the position
Value evaluate(const Position &pos) {
  BoardState *st = pos.state();

  if constexpr (DEFAULT_USE_NNUE) {
    Value nnue = evaluate_nnue(pos);

    Value v = nnue * 5 / 4 + 28;

    v = v * (100 - st->fiftyMove) / 100;

    v = std::clamp(v, -VAL_MATE_BOUND + 1, VAL_MATE_BOUND - 1);

    return v / 2;
  } else {
    Score psq = pos.psq();

    int mgPhase = st->gamePhase;
    if (mgPhase > 24)
      mgPhase = 24;
    int egPhase = 24 - mgPhase;

    Value v = (psq.first * mgPhase + psq.second * egPhase) / 24;

    return pos.getSideToMove() == WHITE ? v : -v;
  }
}

} // namespace Maestro::Eval
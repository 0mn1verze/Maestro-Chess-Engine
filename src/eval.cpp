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

// clang-format off
constexpr Score bonus[PIECE_TYPE_N][SQ_N] = {
    {}, // No piece
    {
    _S(0  ,0  ), _S(0  ,0  ), _S(0  ,0  ), _S(0  ,0  ), _S(0  ,0  ), _S(0  ,0  ), _S(0  ,0  ), _S(0  ,0  ), 
    _S(-35,13 ), _S(-1 ,8  ), _S(-20,8  ), _S(-23,10 ), _S(-15,13 ), _S(24 ,0  ), _S(38 ,2  ), _S(-22,-7 ), 
    _S(-26,4  ), _S(-4 ,7  ), _S(-4 ,-6 ), _S(-10,1  ), _S(3  ,0  ), _S(3  ,-5 ), _S(33 ,-1 ), _S(-12,-8 ), 
    _S(-27,13 ), _S(-2 ,9  ), _S(-5 ,-3 ), _S(12 ,-7 ), _S(17 ,-7 ), _S(6  ,-8 ), _S(10 ,3  ), _S(-25,-1 ), 
    _S(-14,32 ), _S(13 ,24 ), _S(6  ,13 ), _S(21 ,5  ), _S(23 ,-2 ), _S(12 ,4  ), _S(17 ,17 ), _S(-23,17 ), 
    _S(-6 ,94 ), _S(7  ,100), _S(26 ,85 ), _S(31 ,67 ), _S(65 ,56 ), _S(56 ,53 ), _S(25 ,82 ), _S(-20,84 ), 
    _S(98 ,178), _S(134,173), _S(61 ,158), _S(95 ,134), _S(68 ,147), _S(126,132), _S(34 ,165), _S(-11,187), 
    _S(0  ,0  ), _S(0  ,0  ), _S(0  ,0  ), _S(0  ,0  ), _S(0  ,0  ), _S(0  ,0  ), _S(0  ,0  ), _S(0  ,0  ), 
    }, // Pawn
    {
    _S(-105,-29), _S(-21,-51), _S(-58,-23), _S(-33,-15), _S(-17,-22), _S(-28,-18), _S(-19,-50), _S(-23,-64), 
    _S(-29,-42), _S(-53,-20), _S(-12,-10), _S(-3 ,-5 ), _S(-1 ,-2 ), _S(18 ,-20), _S(-14,-23), _S(-19,-44), 
    _S(-23,-23), _S(-9 ,-3 ), _S(12 ,-1 ), _S(10 ,15 ), _S(19 ,10 ), _S(17 ,-3 ), _S(25 ,-20), _S(-16,-22), 
    _S(-13,-18), _S(4  ,-6 ), _S(16 ,16 ), _S(13 ,25 ), _S(28 ,16 ), _S(19 ,17 ), _S(21 ,4  ), _S(-8 ,-18), 
    _S(-9 ,-17), _S(17 ,3  ), _S(19 ,22 ), _S(53 ,22 ), _S(37 ,22 ), _S(69 ,11 ), _S(18 ,8  ), _S(22 ,-18), 
    _S(-47,-24), _S(60 ,-20), _S(37 ,10 ), _S(65 ,9  ), _S(84 ,-1 ), _S(129,-9 ), _S(73 ,-19), _S(44 ,-41), 
    _S(-73,-25), _S(-41,-8 ), _S(72 ,-25), _S(36 ,-2 ), _S(23 ,-9 ), _S(62 ,-25), _S(7  ,-24), _S(-17,-52), 
    _S(-167,-58), _S(-89,-38), _S(-34,-13), _S(-49,-28), _S(61 ,-31), _S(-97,-27), _S(-15,-63), _S(-107,-99), 
    }, // Knight
    {
    _S(-33,-23), _S(-3 ,-9 ), _S(-14,-23), _S(-21,-5 ), _S(-13,-9 ), _S(-12,-16), _S(-39,-5 ), _S(-21,-17), 
    _S(4  ,-14), _S(15 ,-18), _S(16 ,-7 ), _S(0  ,-1 ), _S(7  ,4  ), _S(21 ,-9 ), _S(33 ,-15), _S(1  ,-27), 
    _S(0  ,-12), _S(15 ,-3 ), _S(15 ,8  ), _S(15 ,10 ), _S(14 ,13 ), _S(27 ,3  ), _S(18 ,-7 ), _S(10 ,-15), 
    _S(-6 ,-6 ), _S(13 ,3  ), _S(13 ,13 ), _S(26 ,19 ), _S(34 ,7  ), _S(12 ,10 ), _S(10 ,-3 ), _S(4  ,-9 ), 
    _S(-4 ,-3 ), _S(5  ,9  ), _S(19 ,12 ), _S(50 ,9  ), _S(37 ,14 ), _S(37 ,10 ), _S(7  ,3  ), _S(-2 ,2  ),
    _S(-16,2  ), _S(37 ,-8 ), _S(43 ,0  ), _S(40 ,-1 ), _S(35 ,-2 ), _S(50 ,6  ), _S(37 ,0  ), _S(-2 ,4  ),
    _S(-26,-8 ), _S(16 ,-4 ), _S(-18,7  ), _S(-13,-12), _S(30 ,-3 ), _S(59 ,-13), _S(18 ,-4 ), _S(-47,-14),
    _S(-29,-14), _S(4  ,-21), _S(-82,-11), _S(-37,-8 ), _S(-25,-7 ), _S(-42,-9 ), _S(7  ,-17), _S(-8 ,-24),
    }, // Bishop
    {
    _S(-19,-9 ), _S(-13,2  ), _S(1  ,3  ), _S(17 ,-1 ), _S(16 ,-5 ), _S(7  ,-13), _S(-37,4  ), _S(-26,-20),
    _S(-44,-6 ), _S(-16,-6 ), _S(-20,0  ), _S(-9 ,2  ), _S(-1 ,-9 ), _S(11 ,-9 ), _S(-6 ,-11), _S(-71,-3 ),
    _S(-45,-4 ), _S(-25,0  ), _S(-16,-5 ), _S(-17,-1 ), _S(3  ,-7 ), _S(0  ,-12), _S(-5 ,-8 ), _S(-33,-16),
    _S(-36,3  ), _S(-26,5  ), _S(-12,8  ), _S(-1 ,4  ), _S(9  ,-5 ), _S(-7 ,-6 ), _S(6  ,-8 ), _S(-23,-11),
    _S(-24,4  ), _S(-11,3  ), _S(7  ,13 ), _S(26 ,1  ), _S(24 ,2  ), _S(35 ,1  ), _S(-8 ,-1 ), _S(-20,2  ),
    _S(-5 ,7  ), _S(19 ,7  ), _S(26 ,7  ), _S(36 ,5  ), _S(17 ,4  ), _S(45 ,-3 ), _S(61 ,-5 ), _S(16 ,-3 ),
    _S(27 ,11 ), _S(32 ,13 ), _S(58 ,13 ), _S(62 ,11 ), _S(80 ,-3 ), _S(67 ,3  ), _S(26 ,8  ), _S(44 ,3  ),
    _S(32 ,13 ), _S(42 ,10 ), _S(32 ,18 ), _S(51 ,15 ), _S(63 ,12 ), _S(9  ,12 ), _S(31 ,8  ), _S(43 ,5  ),
    }, // Rook
    {
    _S(-1 ,-33), _S(-18,-28), _S(-9 ,-22), _S(10 ,-43), _S(-15,-5 ), _S(-25,-32), _S(-31,-20), _S(-50,-41),
    _S(-35,-22), _S(-8 ,-23), _S(11 ,-30), _S(2  ,-16), _S(8  ,-16), _S(15 ,-23), _S(-3 ,-36), _S(1  ,-32),
    _S(-14,-16), _S(2  ,-27), _S(-11,15 ), _S(-2 ,6  ), _S(-5 ,9  ), _S(2  ,17 ), _S(14 ,10 ), _S(5  ,5  ),
    _S(-9 ,-18), _S(-26,28 ), _S(-9 ,19 ), _S(-10,47 ), _S(-2 ,31 ), _S(-4 ,34 ), _S(3  ,39 ), _S(-3 ,23 ),
    _S(-27,3  ), _S(-27,22 ), _S(-16,24 ), _S(-16,45 ), _S(-1 ,57 ), _S(17 ,40 ), _S(-2 ,57 ), _S(1  ,36 ),
    _S(-13,-20), _S(-17,6  ), _S(7  ,9  ), _S(8  ,49 ), _S(29 ,47 ), _S(56 ,35 ), _S(47 ,19 ), _S(57 ,9  ),
    _S(-24,-17), _S(-39,20 ), _S(-5 ,32 ), _S(1  ,41 ), _S(-16,58 ), _S(57 ,25 ), _S(28 ,30 ), _S(54 ,0  ),
    _S(-28,-9 ), _S(0  ,22 ), _S(29 ,22 ), _S(12 ,27 ), _S(59 ,27 ), _S(44 ,19 ), _S(43 ,10 ), _S(45 ,20 ),
    }, // Queen
    {
    _S(-15,-53), _S(36 ,-34), _S(12 ,-21), _S(-54,-11), _S(8  ,-28), _S(-28,-14), _S(24 ,-24), _S(14 ,-43),
    _S(1  ,-27), _S(7  ,-11), _S(-8 ,4  ), _S(-64,13 ), _S(-43,14 ), _S(-16,4  ), _S(9  ,-5 ), _S(8  ,-17),
    _S(-14,-19), _S(-14,-3 ), _S(-22,11 ), _S(-46,21 ), _S(-44,23 ), _S(-30,16 ), _S(-15,7  ), _S(-27,-9 ),
    _S(-49,-18), _S(-1 ,-4 ), _S(-27,21 ), _S(-39,24 ), _S(-46,27 ), _S(-44,23 ), _S(-33,9  ), _S(-51,-11),
    _S(-17,-8 ), _S(-20,22 ), _S(-12,24 ), _S(-27,27 ), _S(-30,26 ), _S(-25,33 ), _S(-14,26 ), _S(-36,3  ),
    _S(-9 ,10 ), _S(24 ,17 ), _S(2  ,23 ), _S(-16,15 ), _S(-20,20 ), _S(6  ,45 ), _S(22 ,44 ), _S(-22,13 ),
    _S(29 ,-12), _S(-1 ,17 ), _S(-20,14 ), _S(-7 ,17 ), _S(-8 ,17 ), _S(-4 ,38 ), _S(-38,23 ), _S(-29,11 ),
    _S(-65,-74), _S(23 ,-35), _S(16 ,-18), _S(-15,-18), _S(-56,-11), _S(-34,15 ), _S(2  ,4  ), _S(13 ,-17),
    }, // King
};

// clang-format on

// Init piece square table
void initEval() {
  for (PieceType pt = PAWN; pt <= KING; ++pt) {
    for (Square sq = A1; sq <= H8; ++sq) {
      psqt[toPiece(WHITE, pt)][sq] = bonus[pt][sq] + pieceBonus[pt];
      psqt[toPiece(BLACK, pt)][flipRank(sq)] = -psqt[toPiece(WHITE, pt)][sq];
    }
  }
}

/******************************************\
|==========================================|
|              NNUE Evaluation             |
|             (Stockfish NNUE)             |
|==========================================|
\******************************************/

// convert BBC piece code to Stockfish piece codes
int nnuePieces[PIECE_N] = {blank, wpawn,  wknight, wbishop, wrook,   wqueen,
                           wking, blank,  blank,   bpawn,   bknight, bbishop,
                           brook, bqueen, bking,   blank};

int toNNUEPiece(Piece piece) { return nnuePieces[piece]; }

inline Value evaluate_nnue(const Position &pos) {
  Bitboard bitboard;
  Square sq;
  int pieces[33];
  int squares[33];
  int index = 0;

  // Set king squares
  pieces[index] = nnuePieces[wK];
  squares[index++] = pos.square<KING>(WHITE);

  pieces[index] = nnuePieces[bK];
  squares[index++] = pos.square<KING>(BLACK);

  for (PieceType pt = PAWN; pt <= QUEEN; ++pt) {
    const Square *wpl = pos.squares(WHITE, pt);
    while (*wpl != NO_SQ) {
      pieces[index] = nnuePieces[toPiece(WHITE, pt)];
      squares[index++] = *wpl++;
    }

    const Square *bpl = pos.squares(BLACK, pt);
    while (*bpl != NO_SQ) {
      pieces[index] = nnuePieces[toPiece(BLACK, pt)];
      squares[index++] = *bpl++;
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

    return nnue_evaluate_incremental(pos.sideToMove(), pieces, squares, data);
  } else
    return nnue_evaluate(pos.sideToMove(), pieces, squares);
}

// Evaluate the position
Value evaluate(const Position &pos) {

  Value nnue = evaluate_nnue(pos);

  Value v = nnue * 5 / 4 + 28;

  v = v * (100 - pos.fiftyMove()) / 100;

  v = std::clamp(v, -VAL_MATE_BOUND + 1, VAL_MATE_BOUND - 1);

  return v;

  // Score psq = pos.psq();

  // int mgPhase = std::min(pos.gamePhase(), 24);

  // int egPhase = 24 - mgPhase;

  // Value v = (psq.first * mgPhase + psq.second * egPhase) / 24;

  // return pos.sideToMove() == WHITE ? v : -v;
}

} // namespace Maestro::Eval
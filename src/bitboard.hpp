#ifndef BITBOARD_HPP
#define BITBOARD_HPP

#include <immintrin.h>
#include <vector>

#include "defs.hpp"
#include "utils.hpp"

namespace Maestro {

/******************************************\
|==========================================|
|           Bitboard functions             |
|==========================================|
\******************************************/

// Bitboard initialization function
void initBitboards();

// Bitboard print function
void printBitboard(Bitboard bb);

// U32 the number of set bits in a bitboard
inline int countBits(Bitboard bb) { return _mm_popcnt_u64(bb); }

// Get the least significant bit from a bitboard
inline Square getLSB(Bitboard bb) { return (Square)_tzcnt_u64(bb); }

// Get the most significant bit from a bitboard
inline Square getMSB(Bitboard bb) { return (Square)(63 ^ _lzcnt_u64(bb)); }

// Pop the least significant bit from a bitboard
inline Square popLSB(Bitboard &bb) {
  Square lsb = getLSB(bb);
  bb = _blsr_u64(bb);
  return lsb;
}

// Parallel bit extraction
inline Square pext(Bitboard bb, Bitboard mask) {
  return (Square)_pext_u64(bb, mask);
}

// More than one bit set
inline bool moreThanOne(Bitboard bb) { return bb & (bb - 1); }

/******************************************\
|==========================================|
|             Type Conversions             |
|==========================================|
\******************************************/

// Get square bitboard
constexpr Bitboard squareBB(Square sq) { return 1ULL << sq; }
// Get file bitboard
constexpr Bitboard fileBB(File f) { return 0x0101010101010101ULL << f; }
// Get rank bitboard (square)
constexpr Bitboard fileBB(Square sq) { return fileBB(fileOf(sq)); }
// Get rank bitboard
constexpr Bitboard rankBB(Rank r) { return 0xFFULL << (r << 3); }
// Get rank bitboard (square)
constexpr Bitboard rankBB(Square sq) { return rankBB(rankOf(sq)); }
// File bitboards (variadic)
template <typename... Bitboards>
constexpr inline Bitboard fileBB(Bitboard file, Bitboards... files) {
  return file | fileBB(files...);
}
// Rank bitboards (variadic)
template <typename... Bitboards>
constexpr inline Bitboard rankBB(Bitboard rank, Bitboards... ranks) {
  return rank | rankBB(ranks...);
}

/******************************************\
|==========================================|
|           Bitboard Constants             |
|==========================================|
\******************************************/

// File bitboards
constexpr Bitboard FILE_ABB = fileBB(FILE_A);
constexpr Bitboard FILE_BBB = fileBB(FILE_B);
constexpr Bitboard FILE_CBB = fileBB(FILE_C);
constexpr Bitboard FILE_DBB = fileBB(FILE_D);
constexpr Bitboard FILE_EBB = fileBB(FILE_E);
constexpr Bitboard FILE_FBB = fileBB(FILE_F);
constexpr Bitboard FILE_GBB = fileBB(FILE_G);
constexpr Bitboard FILE_HBB = fileBB(FILE_H);
// Rank bitboards
constexpr Bitboard RANK_1BB = rankBB(RANK_1);
constexpr Bitboard RANK_2BB = rankBB(RANK_2);
constexpr Bitboard RANK_3BB = rankBB(RANK_3);
constexpr Bitboard RANK_4BB = rankBB(RANK_4);
constexpr Bitboard RANK_5BB = rankBB(RANK_5);
constexpr Bitboard RANK_6BB = rankBB(RANK_6);
constexpr Bitboard RANK_7BB = rankBB(RANK_7);
constexpr Bitboard RANK_8BB = rankBB(RANK_8);
// Empty and full bitboards
constexpr Bitboard EMPTYBB = 0ULL;
constexpr Bitboard FULLBB = ~EMPTYBB;
// Light and dark squares
constexpr Bitboard DarkSquares = 0xAA55AA55AA55AA55ULL;
// Bitboards for center, king side and queen side
constexpr Bitboard QueenSide = FILE_ABB | FILE_BBB | FILE_CBB | FILE_DBB;
constexpr Bitboard CenterFiles = FILE_CBB | FILE_DBB | FILE_EBB | FILE_FBB;
constexpr Bitboard KingSide = FILE_EBB | FILE_FBB | FILE_GBB | FILE_HBB;
constexpr Bitboard Center = (FILE_EBB | FILE_FBB) & (RANK_4BB | RANK_5BB);
// King flank bitboards
constexpr Bitboard KingFlank[FILE_N] = {
    QueenSide ^ FILE_DBB, QueenSide, QueenSide, CenterFiles,
    CenterFiles,          KingSide,  KingSide,  KingSide ^ FILE_EBB};

/******************************************\
|==========================================|
|           Bitboard Operators             |
|==========================================|
\******************************************/

// Bitwise AND operation on bitboards (Get Bit)
constexpr Bitboard operator&(Bitboard bb, Square sq) {
  return bb & squareBB(sq);
}
// Bitwise OR opeartor on bitboards (Set bit)
constexpr Bitboard operator|(Bitboard bb, Square sq) {
  return bb | squareBB(sq);
}
// Bitwise XOR operator on bitboards (Toggle bit)
constexpr Bitboard operator^(Bitboard bb, Square sq) {
  return bb ^ squareBB(sq);
}
// Bitwise OR opeartor on bitboards (Combine squares)
constexpr Bitboard operator|(Square sq1, Square sq2) {
  return squareBB(sq1) | squareBB(sq2);
}
// Bitwise assignment OR operator on bitboards (Set Bit)
constexpr Bitboard &operator|=(Bitboard &bb, Square sq) {
  return bb |= squareBB(sq);
}
// Bitwise assignment XOR operator on bitboards (Toggle Bit)
constexpr Bitboard &operator^=(Bitboard &bb, Square sq) {
  return bb ^= squareBB(sq);
}

/******************************************\
|==========================================|
|              Lookup Tables               |
|==========================================|
\******************************************/

struct Magic {
  // Pointer to attacks table block
  Bitboard *attacks;
  // Attack mask for slider piece on a particular square
  Bitboard mask;
  // Calculate index in attacks table
  unsigned int index(Bitboard occupied) { return pext(occupied, mask); }
  // Get attacks bitboard for a particular occupancy
  Bitboard operator[](Bitboard occupied) { return attacks[index(occupied)]; }
};

// Pseudo attacks for all pieces except pawns
// extern Bitboard pseudoAttacks[PIECE_TYPE_N][SQ_N];
extern Array<Bitboard, PIECE_TYPE_N, SQ_N> pseudoAttacks;
// Pseudo attacks for pawns
extern Array<Bitboard, COLOUR_N, SQ_N> pawnAttacks;
// Bishop magics
extern Array<Magic, SQ_N> bishopAttacks;
// Rook magics
extern Array<Magic, SQ_N> rookAttacks;
// Bishop attack tables
extern Bitboard bishopTable[0x1480];
// Rook attack tables
extern Bitboard rookTable[0x19000];
// Line the two squares lie on [from][to]
extern Array<Bitboard, SQ_N, SQ_N> lineBB;
// In between bitboards [from][to]
extern Array<Bitboard, SQ_N, SQ_N> betweenBB;
// Pins [king][attacker]
extern Array<Bitboard, SQ_N, SQ_N> pinBB;
// Checks [king][attacker]
extern Array<Bitboard, SQ_N, SQ_N> checkBB;
// Castling rights lookup table
extern Array<Castling, SQ_N> castlingRights;

/******************************************\
|==========================================|
|               Directions                 |
|==========================================|
\******************************************/

// Shift bitboards
template <Direction d> constexpr inline Bitboard shift(Bitboard bb) {
  // Returns the bitboard shift with checking for edges
  switch (d) {
  case N:
    return bb << 8;
  case S:
    return bb >> 8;
  case E:
    return (bb & ~fileBB(FILE_H)) << 1;
  case W:
    return (bb & ~fileBB(FILE_A)) >> 1;
  case NE:
    return (bb & ~fileBB(FILE_H)) << 9;
  case NW:
    return (bb & ~fileBB(FILE_A)) << 7;
  case SE:
    return (bb & ~fileBB(FILE_H)) >> 7;
  case SW:
    return (bb & ~fileBB(FILE_A)) >> 9;
  case NN:
    return bb << 16;
  case SS:
    return bb >> 16;
  case NNE:
    return (bb & ~fileBB(FILE_H)) << 17;
  case NNW:
    return (bb & ~fileBB(FILE_A)) << 15;
  case NEE:
    return (bb & ~fileBB(FILE_H, FILE_G)) << 10;
  case NWW:
    return (bb & ~fileBB(FILE_A, FILE_B)) << 6;
  case SEE:
    return (bb & ~fileBB(FILE_H, FILE_G)) >> 6;
  case SWW:
    return (bb & ~fileBB(FILE_A, FILE_B)) >> 10;
  case SSE:
    return (bb & ~fileBB(FILE_H)) >> 15;
  case SSW:
    return (bb & ~fileBB(FILE_A)) >> 17;
  default:
    return 0ULL;
  }
}

constexpr Bitboard shift(Square sq, Direction d) {
  Square to = sq + d;
  return (isValid(to) && isShift(sq, to)) ? squareBB(to) : Bitboard(0);
}

constexpr Direction direction(Square from, Square to) {
  // Rank and file distance
  int rankDist = rankOf(from) - rankOf(to);
  int fileDist = fileOf(from) - fileOf(to);
  // Check if the squares are on the anti diagonal
  if (rankDist > 0) {
    if (fileDist)
      return (fileDist > 0) ? SW : SE;
    return S;
  } else if (rankDist < 0) {
    if (fileDist)
      return (fileDist > 0) ? NW : NE;
    return N;
  }
  if (fileDist)
    return (fileDist > 0) ? W : E;
  return Direction(0);
}

/******************************************\
|==========================================|
|              Attack Lookup               |
|==========================================|
\******************************************/

// Get attacks bitboard for a piece on a square (With occupancy)
template <PieceType pt>
Bitboard attacksBB(Square sq, Bitboard occupied = 0ULL) {
  switch (pt) {
  case KNIGHT:
    return pseudoAttacks[KNIGHT][sq];
  case KING:
    return pseudoAttacks[KING][sq];
  case BISHOP:
    return bishopAttacks[sq][occupied];
  case ROOK:
    return rookAttacks[sq][occupied];
  case QUEEN:
    return bishopAttacks[sq][occupied] | rookAttacks[sq][occupied];
  default:
    return Bitboard(0);
  }
}

// Attacks BB function (Non template version)
inline Bitboard attacksBB(PieceType pt, Square sq, Bitboard occupied = 0ULL) {
  switch (pt) {
  case KNIGHT:
    return pseudoAttacks[KNIGHT][sq];
  case KING:
    return pseudoAttacks[KING][sq];
  case BISHOP:
    return bishopAttacks[sq][occupied];
  case ROOK:
    return rookAttacks[sq][occupied];
  case QUEEN:
    return bishopAttacks[sq][occupied] | rookAttacks[sq][occupied];
  default:
    return Bitboard(0);
  }
}

// Get pawn attacks bitboard (Entire bitboard)
template <Colour c> inline Bitboard pawnAttacksBB(Bitboard bb) {
  return (c == WHITE) ? shift<NW>(bb) | shift<NE>(bb)
                      : shift<SW>(bb) | shift<SE>(bb);
}

inline Bitboard pawnAttacksBB(Colour c, Bitboard bb) {
  return (c == WHITE) ? pawnAttacksBB<WHITE>(bb) : pawnAttacksBB<BLACK>(bb);
}

// Get double pawn attacks bitboard (Entire bitboard)
template <Colour c> inline Bitboard doublePawnAttacksBB(Bitboard bb) {
  return (c == WHITE) ? shift<NW>(bb) & shift<NE>(bb)
                      : shift<SW>(bb) & shift<SE>(bb);
}

// Get pawn attacks bitboard (Single square)
template <Colour c> Bitboard pawnAttacksBB(Square sq) {
  return pawnAttacks[c][sq];
}

// Get pawn attacks bitboard
inline Bitboard pawnAttacksBB(Colour c, Square sq) {
  return pawnAttacks[c][sq];
}

// Get adjacent files bitboard
constexpr Bitboard adjacentFilesBB(Square sq) {
  return shift<E>(squareBB(sq)) | shift<W>(squareBB(sq));
}

// Get forward ranks bitboard
constexpr Bitboard forwardRanksBB(Colour c, Square sq) {
  return (c == WHITE) ? ~RANK_1BB << 8 * rankOf(sq)
                      : ~RANK_8BB >> 8 * (RANK_8 - rankOf(sq));
}

// Get forward file bitboard
constexpr Bitboard forwardFileBB(Colour c, Square sq) {
  return forwardRanksBB(c, sq) & fileBB(sq);
}

// Get passed pawn bitboard
constexpr Bitboard passedPawnSpan(Colour c, Square sq) {
  return forwardFileBB(c, sq) & (adjacentFilesBB(sq) | fileBB(sq));
}

// Check if 3 squares are collinear
constexpr bool aligned(Square sq1, Square sq2, Square sq3) {
  return lineBB[sq1][sq2] & squareBB(sq3);
}

// Get front most square
constexpr Square frontMostSquare(Colour c, Bitboard bb) {
  return (c == WHITE) ? getMSB(bb) : getLSB(bb);
}

constexpr Bitboard sameColourSquares(Square sq) {
  return (DarkSquares & sq) ? DarkSquares : ~DarkSquares;
}

} // namespace Maestro

#endif // BITBOARD_HPP
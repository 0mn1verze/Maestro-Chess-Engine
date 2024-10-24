#ifndef BITBOARD_HPP
#define BITBOARD_HPP

#define USE_INTRINSICS 1

// C++ Standard Library
#if defined(USE_INTRINSICS)
#include <immintrin.h>
#endif

#include "defs.hpp"

/******************************************\
|==========================================|
|           Bitboard functions             |
|==========================================|
\******************************************/

// Bitboard initialization function
void initBitboards();

// Bitboard print function
void printBitboard(Bitboard bb);

// Count the number of set bits in a bitboard
inline int countBits(Bitboard bb) { return __builtin_popcountll(bb); }

// Get the least significant bit from a bitboard
inline Square getLSB(Bitboard bb) { return (Square)__builtin_ctzll(bb); }

// Get the most significant bit from a bitboard
inline Square getMSB(Bitboard bb) { return (Square)(63 ^ __builtin_clzll(bb)); }

// Pop the least significant bit from a bitboard
inline Square popLSB(Bitboard &bb) {
  Square lsb = getLSB(bb);
#if defined(USE_INTRINISICS)
  bb = _blsr_u64(bb);
#else
  bb &= bb - 1;
#endif
  return lsb;
}

#if defined(USE_INTRINSICS)
// Parallel bit extraction
inline unsigned pext(Bitboard bb, Bitboard mask) {
  return (unsigned)_pext_u64(bb, mask);
}
#endif

// More than one bit set
inline bool moreThanOne(Bitboard bb) { return bb & (bb - 1); }

/******************************************\
|==========================================|
|           Bitboard Constants             |
|==========================================|
\******************************************/

// Bitboard constants
constexpr Bitboard FILEABB = 0x0101010101010101ULL;
constexpr Bitboard RANK1BB = 0xFFULL;
constexpr Bitboard EMPTYBB = 0ULL;
constexpr Bitboard FULLBB = ~EMPTYBB;
constexpr Bitboard DarkSquares = 0xAA55AA55AA55AA55ULL;

// CPU flag
#if defined(USE_INTRINSICS)
constexpr bool HasPext = true;
#else
constexpr bool HasPext = false;
#endif

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
  // Magic number for the square
  Bitboard magic;
  // Shift right to get index
  int shift;
  // Calculate index in attacks table
  unsigned int index(Bitboard occupied) {
#if defined(USE_INTRINSICS)
    return pext(occupied, mask);
#else
    return unsigned(((occupied & mask) * magic) >> shift);
#endif
  }

  Bitboard operator[](Bitboard occupied) { return attacks[index(occupied)]; }
};

// Pseudo attacks for all pieces except pawns
extern Bitboard pseudoAttacks[PIECE_TYPE_N][SQ_N];

// Bishop and rook magics
extern Magic bishopAttacks[SQ_N];
extern Magic rookAttacks[SQ_N];

// Bishop and rook attack tables
extern Bitboard bishopTable[0x1480];
extern Bitboard rookTable[0x19000];

// Line the two squares lie on [from][to]
extern Bitboard lineBB[SQ_N][SQ_N];
extern Bitboard betweenBB[SQ_N][SQ_N];

// Pins and checks [king][attacker]
extern Bitboard pinBB[SQ_N][SQ_N];
extern Bitboard checkBB[SQ_N][SQ_N];

// Castling rights lookup table
extern Castling castlingRights[SQ_N];

/******************************************\
|==========================================|
|           Bitboard Operators             |
|==========================================|
\******************************************/

// Get square bitboard
constexpr Bitboard squareBB(Square sq) { return 1ULL << sq; }
// Get rank bitboard
constexpr Bitboard rankBB(Rank r) { return RANK1BB << (r << 3); }
// Get rank bitboard (square)
constexpr Bitboard rankBB(Square sq) { return rankBB(rankOf(sq)); }
// Get file bitboard
constexpr Bitboard fileBB(File f) { return FILEABB << f; }
// Get rank bitboard (square)
constexpr Bitboard fileBB(Square sq) { return fileBB(fileOf(sq)); }
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
|           Bitboard Constants             |
|==========================================|
\******************************************/

constexpr Bitboard QueenSide =
    fileBB(FILE_A) | fileBB(FILE_B) | fileBB(FILE_C) | fileBB(FILE_D);
constexpr Bitboard CenterFiles =
    fileBB(FILE_C) | fileBB(FILE_D) | fileBB(FILE_E) | fileBB(FILE_F);
constexpr Bitboard KingSide =
    fileBB(FILE_E) | fileBB(FILE_F) | fileBB(FILE_G) | fileBB(FILE_H);
constexpr Bitboard Center =
    (fileBB(FILE_D) | fileBB(FILE_E)) & (rankBB(RANK_4) | rankBB(RANK_5));

constexpr Bitboard KingFlank[FILE_N] = {QueenSide ^ fileBB(FILE_D),
                                        QueenSide,
                                        QueenSide,
                                        CenterFiles,
                                        CenterFiles,
                                        KingSide,
                                        KingSide,
                                        KingSide ^ fileBB(FILE_E)};

/******************************************\
|==========================================|
|            Bitboard Helpers              |
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
    return (bb & ~(fileBB(FILE_H) | fileBB(FILE_G))) << 10;
  case NWW:
    return (bb & ~(fileBB(FILE_A) | fileBB(FILE_B))) << 6;
  case SEE:
    return (bb & ~(fileBB(FILE_H) | fileBB(FILE_G))) >> 6;
  case SWW:
    return (bb & ~(fileBB(FILE_A) | fileBB(FILE_B))) >> 10;
  case SSE:
    return (bb & ~fileBB(FILE_H)) >> 15;
  case SSW:
    return (bb & ~fileBB(FILE_A)) >> 17;
  default:
    return 0ULL;
  }
}

int rankDist(Square sq1, Square sq2);

int fileDist(Square sq1, Square sq2);

/******************************************\
|==========================================|
|              Attack Lookup               |
|==========================================|
\******************************************/

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

template <Colour c> Bitboard pawnAttacksBB(Bitboard bb) {
  return (c == WHITE) ? shift<NW>(bb) | shift<NE>(bb)
                      : shift<SW>(bb) | shift<SE>(bb);
}

template <Colour c> Bitboard pawnAttacksBB(Square sq) {
  return pawnAttacksBB<c>(squareBB(sq));
}

// Files adjacet to the pawn
constexpr Bitboard adjacentFilesBB(Square sq) {
  return shift<E>(fileBB(sq)) | shift<W>(fileBB(sq));
}

// Forward ranks (Bitboard representing the squares in front of the pawn)
constexpr Bitboard forwardRanksBB(Colour c, Square sq) {
  return (c == WHITE) ? ~RANK1BB << 8 * (rankOf(sq) - RANK_1)
                      : ~rankBB(RANK_8) >> 8 * (RANK_8 - rankOf(sq));
}

// Forward files (Bitboard representing the squares to the left and right of the
// pawn)
constexpr Bitboard forwardFilesBB(Colour c, Square sq) {
  return forwardRanksBB(c, sq) & fileBB(sq);
}

// Pawn attacks span (Bitboard represetning all the squares that can be attacked
// by a pawn)
constexpr Bitboard pawnAttacksSpanBB(Colour c, Square sq) {
  return forwardRanksBB(c, sq) & adjacentFilesBB(sq);
}

// Passed pawn span (Bitboard representing passed pawn mask)
constexpr Bitboard passedPawnSpanBB(Colour c, Square sq) {
  return forwardRanksBB(c, sq) & (adjacentFilesBB(sq) | fileBB(sq));
}

// Pawn double attacks
constexpr Bitboard pawnDoubleAttacksBB(Colour c, Bitboard bb) {
  return (c == WHITE) ? shift<NW>(bb) & shift<NE>(bb)
                      : shift<SW>(bb) & shift<SE>(bb);
}

template <class T>
constexpr const T &clamp(const T &v, const T &lo, const T &hi) {
  return (v < lo) ? lo : (hi < v) ? hi : v;
}

inline Square frontMostSq(Colour c, Bitboard bb) {
  return (c == WHITE) ? getMSB(bb) : getLSB(bb);
}

constexpr Bitboard lowRanks(Colour side) {
  return (side == WHITE) ? rankBB(RANK_2) | rankBB(RANK_3)
                         : rankBB(RANK_7) | rankBB(RANK_6);
}

constexpr Bitboard outPostRanks(Colour c) {
  return (c == WHITE) ? rankBB(RANK_4) | rankBB(RANK_5) | rankBB(RANK_6)
                      : rankBB(RANK_5) | rankBB(RANK_4) | rankBB(RANK_3);
}

constexpr Bitboard sameColourSquares(Square sq) {
  return (DarkSquares & sq) ? DarkSquares : ~DarkSquares;
}

inline bool aligned(Square sq1, Square sq2, Square sq3) {
  return lineBB[sq1][sq2] & squareBB(sq3);
}

#endif // BITBOARD_HPP
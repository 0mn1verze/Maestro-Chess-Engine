#ifndef DEFS_HPP
#define DEFS_HPP

// C++ Standard Library
#include <cstdint>
#include <ostream>

/******************************************\
|==========================================|
|            User Defined Types            |
|==========================================|
\******************************************/

using Bitboard = std::uint64_t;
using Key = std::uint64_t;
using Count = std::int64_t;
using Time = std::uint32_t;
using Value = std::int32_t;
using Depth = std::int8_t;

/******************************************\
|==========================================|
|             Board Constants              |
|==========================================|
\******************************************/

constexpr Depth MAX_DEPTH = 64;
constexpr int MAX_MOVES = 256;
constexpr Time MOVE_OVERHEAD = 1000;
constexpr Value VAL_INFINITE = 50000;
constexpr Value VAL_MATE = 49000;
constexpr Value VAL_MATE_BOUND = 48000;
constexpr Value VAL_NONE = VAL_MATE + 1;

/******************************************\
|==========================================|
|             Board Enum Types             |
|==========================================|
\******************************************/

// clang-format off

/*
    Position representation on the board
*/

// Squares on the board
enum Square {
    A1, B1, C1, D1, E1, F1, G1, H1,
    A2, B2, C2, D2, E2, F2, G2, H2,
    A3, B3, C3, D3, E3, F3, G3, H3,
    A4, B4, C4, D4, E4, F4, G4, H4,
    A5, B5, C5, D5, E5, F5, G5, H5,
    A6, B6, C6, D6, E6, F6, G6, H6,
    A7, B7, C7, D7, E7, F7, G7, H7,
    A8, B8, C8, D8, E8, F8, G8, H8,
    NO_SQ, SQ_N = 64
};

// Files on the board
enum File {
    FILE_A, FILE_B, FILE_C, FILE_D, FILE_E, FILE_F, FILE_G, FILE_H, NO_FILE, FILE_N = 8
};

// Ranks on the board
enum Rank {
    RANK_1, RANK_2, RANK_3, RANK_4, RANK_5, RANK_6, RANK_7, RANK_8, NO_RANK, RANK_N = 8
};

// Board directions (For king, pawn and knight moves)
enum Direction {
    N = 8, S = -8, W = -1, E = 1,
    NE = 9, NW = 7, SE = -7, SW = -9,
    NNE = 17, NNW = 15, NEE = 10, NWW = 6, SEE = -6, SWW = -10, SSE = -15, SSW = -17,
    NN = 16, SS = -16
};

/*
    Piece representation on the board
*/

// Pieces on the board
enum Piece {
    NO_PIECE, wP = 1, wN, wB, wR, wQ, wK, wPieces,
              bP = 8 + wP, bN, bB, bR, bQ, bK, bPieces,
    PIECE_N = 16
};

// Piece types
enum PieceType {
    NO_PIECE_TYPE, PAWN, KNIGHT, BISHOP, ROOK, QUEEN, KING, ALL_PIECES, PIECE_TYPE_N = 8
};

/* 
    Board state representation
*/

// Side to move
enum Colour {
    WHITE, BLACK, BOTH, COLOUR_N = 2
};

// Castling rights
enum Castling {
    WKCA = 1, WQCA = 2, BKCA = 4, BQCA = 8, 
    KING_SIDE = WKCA | BKCA,
    QUEEN_SIDE = WQCA | BQCA,
    WHITE_SIDE = WKCA | WQCA,
    BLACK_SIDE = BKCA | BQCA,
    ANY_SIDE = WHITE_SIDE | BLACK_SIDE,
    NO_CASTLE = 0,
    CASTLING_N = 16
};

enum ScaleFactor {
  SCALE_FACTOR_DRAW    = 0,
  SCALE_FACTOR_NORMAL  = 64,
  SCALE_FACTOR_MAX     = 128,
  SCALE_FACTOR_NONE    = 255
};

/******************************************\
|==========================================|
|             Piece Constants              |
|==========================================|
\******************************************/

constexpr Value VAL_ZERO = 0;
constexpr Value PawnValue = 126;
constexpr Value KnightValue = 781;
constexpr Value BishopValue = 825;
constexpr Value RookValue = 1276;
constexpr Value QueenValue = 2538;

constexpr Value PieceValue[PIECE_N] = {
    VAL_ZERO,  PawnValue,  KnightValue, BishopValue, RookValue,   QueenValue,
    VAL_ZERO,  VAL_ZERO,   VAL_ZERO,    PawnValue,   KnightValue, BishopValue,
    RookValue, QueenValue, VAL_ZERO,    VAL_ZERO};

// clang-format on

/******************************************\
|==========================================|
|            Move representation           |
| Bits (0-5): from square                  |
| Bits (6-11): to square                   |
| Bits (12-13): flags                      |
| Bits (14-15): promoted piece type        |
|==========================================|
\******************************************/

// Move flags
enum MoveFlag { NORMAL = 0, EN_PASSANT = 1, PROMOTION = 2, CASTLE = 3 };

// clang-format on

// Move representation
class Move {
public:
  Move() = default;
  constexpr explicit Move(std::uint16_t m) : data(m), score(0){};
  constexpr Move &operator=(const Move &m) = default;

  template <MoveFlag flag = NORMAL>
  static constexpr inline Move encode(Square from, Square to,
                                      PieceType promoted = KNIGHT) {
    return Move(from | to << 6 | flag << 12 | (promoted - KNIGHT) << 14);
  }

  // Helper functions
  Square from() const { return Square(data & H8); }
  Square to() const { return Square((data >> 6) & H8); }
  PieceType promoted() const { return PieceType((data >> 14) + KNIGHT); }
  int raw() const { return data; }

  // Move flags
  bool isEnPassant() const { return flag() == EN_PASSANT; }
  bool isPromotion() const { return flag() == PROMOTION; }
  bool isCastle() const { return flag() == CASTLE; }
  bool isNormal() const { return flag() == NORMAL; }

  bool isOk() const { return data != none().raw() and data != null().raw(); }

  // Operators
  constexpr bool operator==(const Move &m) const { return data == m.raw(); }
  constexpr bool operator!=(const Move &m) const { return data != m.raw(); }
  constexpr bool operator<(const Move &m) const { return score < m.score; }

  constexpr explicit operator bool() const {
    return data != none().raw() and data != null().raw();
  }

  // No move
  constexpr static Move none() { return Move(0); }
  // Null move
  constexpr static Move null() { return Move::encode(B1, B1); }

  int score; // For move ordering
private:
  MoveFlag flag() const { return MoveFlag((data >> 12) & CASTLE); }
  std::uint16_t data;
};

/******************************************\
|==========================================|
|               Lookup table               |
|                                          |
| dist[sq1][sq2] = max(rankDist, fileDist) |
|==========================================|
\******************************************/

extern int dist[SQ_N][SQ_N];

/******************************************\
|==========================================|
|             Enum Operators               |
|==========================================|
\******************************************/

// Increment operators
constexpr Square &operator++(Square &d) { return d = Square(d + 1); }
constexpr File &operator++(File &d) { return d = File(d + 1); }
constexpr Rank &operator++(Rank &d) { return d = Rank(d + 1); }
constexpr PieceType &operator++(PieceType &d) { return d = PieceType(d + 1); }
constexpr Castling &operator++(Castling &c) { return c = Castling(c + 1); }

// Decrement operators
constexpr Square &operator--(Square &d) { return d = Square(d - 1); }
constexpr File &operator--(File &d) { return d = File(d - 1); }
constexpr Rank &operator--(Rank &d) { return d = Rank(d - 1); }

// Add direction to square
constexpr Square operator+(Square sq, Direction d) {
  return Square(int(sq) + int(d));
}
// Add direction to square
constexpr Square operator-(Square sq, Direction d) {
  return Square(int(sq) - int(d));
}
// Add direction to square
constexpr Square &operator+=(Square &sq, Direction d) { return sq = sq + d; }
// Unary operator direction
constexpr Direction operator-(Direction d) { return Direction(-int(d)); }
// Multiply direction
constexpr Direction operator*(Direction d, int m) {
  return Direction(int(d) * m);
}

// Flip side
constexpr Colour operator~(Colour s) { return Colour(int(s) ^ 1); }

// Bitwise AND operation on Castling Rights
constexpr Castling operator&(Castling c1, Castling c2) {
  return Castling(int(c1) & int(c2));
}
// Bitwise OR operation on Castling Rights
constexpr Castling operator|(Castling c1, Castling c2) {
  return Castling(int(c1) | int(c2));
}
// Bitwise XOR operation on Castling Rights
constexpr Castling operator^(Castling c1, Castling c2) {
  return Castling(int(c1) ^ int(c2));
}
// Bitwise assignment AND operator on Castling Rights
constexpr Castling &operator&=(Castling &c1, Castling c2) {
  return c1 = c1 & c2;
}
// Bitwise assignment OR operation on Castling Rights
constexpr Castling &operator|=(Castling &c1, Castling c2) {
  return c1 = c1 | c2;
}
// Bitwise assignment XOR operation on Castling Rights
constexpr Castling &operator^=(Castling &c1, Castling c2) {
  return c1 = c1 ^ c2;
}
// Bitwise NOT operation on Castling Rights
constexpr Castling operator~(Castling c) { return c ^ ANY_SIDE; }

// Change the output of std::int8_t
inline std::ostream &operator<<(std::ostream &os, std::int8_t c) {
  return os << static_cast<int>(c);
}

// Change the output of std::uint8_t
inline std::ostream &operator<<(std::ostream &os, std::uint8_t c) {
  return os << static_cast<int>(c);
}

/******************************************\
|==========================================|
|             Type conversions             |
|==========================================|
\******************************************/

constexpr Square toSquare(File f, Rank r) {
  return Square(int(f) + (int(r) << 3));
}

constexpr PieceType toPieceType(Piece pce) { return PieceType(pce & wPieces); }

constexpr Colour toColour(Piece pce) { return Colour(pce >> 3); }

constexpr Piece toPiece(Colour c, PieceType pt) { return Piece(c << 3 | pt); }

constexpr Rank rankOf(Square sq) { return Rank(sq >> 3); }

constexpr File fileOf(Square sq) { return File(sq & 7); }

/******************************************\
|==========================================|
|             Type validations             |
|==========================================|
\******************************************/

constexpr bool isSquare(Square sq) { return sq >= A1 && sq <= H8; }

inline bool isShift(Square sq1, Square sq2) { return dist[sq1][sq2] <= 3; }

inline int squareDist(Square sq1, Square sq2) { return dist[sq1][sq2]; }

/******************************************\
|==========================================|
|              Board Helpers               |
|==========================================|
\******************************************/

constexpr Square flipRank(Square sq) { return Square(int(sq) ^ int(A8)); }

constexpr File fileFromEdge(File f) { return std::min(f, File(FILE_H - f)); }

constexpr Rank relativeRank(Colour c, Rank r) { return Rank(r ^ (c * 7)); }

constexpr Rank relativeRank(Colour c, Square sq) {
  return relativeRank(c, rankOf(sq));
}

constexpr Square relativeSquare(Colour c, Square sq) {
  return Square(sq ^ (c * 56));
}

constexpr Rank EPRank(Colour side) { return (side == WHITE) ? RANK_4 : RANK_5; }

constexpr Direction pawnPush(Colour side) { return (side == WHITE) ? N : S; }

constexpr Direction pawnLeft(Colour side) { return (side == WHITE) ? NW : SE; }

constexpr Direction pawnRight(Colour side) { return (side == WHITE) ? NE : SW; }

#endif // DEFS_HPP
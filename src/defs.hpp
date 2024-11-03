#ifndef DEFS_HPP
#define DEFS_HPP

#include <cstdint>
#include <cstdlib>
#include <utility>

namespace Maestro {

/******************************************\
|==========================================|
|             Board U16 Types              |
|==========================================|
\******************************************/

using Bitboard = std::uint64_t;
using U64 = std::uint64_t;
using Key = std::uint64_t;

using U32 = std::uint32_t;

using U16 = std::uint16_t;
using Value = int;

using I16 = std::int16_t;

using Depth = std::uint8_t;

/******************************************\
|==========================================|
|             Board Constants              |
|==========================================|
\******************************************/

constexpr int MAX_DEPTH = 64;
constexpr int MAX_MOVES = 256;
constexpr int MAX_PLY = 128;
constexpr Value VAL_INFINITE = 50000;
constexpr Value VAL_MATE_BOUND = 32000;
constexpr Value VAL_MATE = VAL_MATE_BOUND + MAX_PLY;
constexpr Value VAL_NONE = VAL_MATE + 1;

/******************************************\
|==========================================|
|             Board Enum Types             |
|==========================================|
\******************************************/

// clang-format off

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

// Board directions (For king, pawn and knight moves)
enum Direction {
    N = 8, S = -8, W = -1, E = 1,
    NE = 9, NW = 7, SE = -7, SW = -9,
    NNE = 17, NNW = 15, NEE = 10, NWW = 6, SEE = -6, SWW = -10, SSE = -15, SSW = -17,
    NN = 16, SS = -16
};

// Castling rights
enum Castling : int {
    WK_SIDE = 1, WQ_SIDE = 2, BK_SIDE = 4, BQ_SIDE = 8, 
    KING_SIDE = WK_SIDE | BK_SIDE,
    QUEEN_SIDE = WQ_SIDE | BQ_SIDE,
    WHITE_SIDE = WK_SIDE | WQ_SIDE,
    BLACK_SIDE = BK_SIDE | BQ_SIDE,
    ANY_SIDE = WHITE_SIDE | BLACK_SIDE,
    NO_CASTLE = 0,
    CASTLING_N = 16
};

// Side to move
enum Colour : int {
    WHITE, BLACK, BOTH, COLOUR_N = 2
};

constexpr int CONT_N = 2;

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

/******************************************\
|==========================================|
|           Board Enum Operators           |
|==========================================|
\******************************************/

// Add direction to square
constexpr Square operator+(Square sq, Direction d) { return static_cast<Square>(static_cast<int>(sq) + static_cast<int>(d)); }
// Subtract direction from square
constexpr Square operator-(Square sq, Direction d) { return static_cast<Square>(static_cast<int>(sq) - static_cast<int>(d)); }
// Increment Square operator
constexpr Square &operator++(Square &d) { return d = static_cast<Square>(d + 1); }
// Decrement Square operator
constexpr Square &operator--(Square &d) { return d = static_cast<Square>(d - 1); }
// Add direction to square
constexpr Square &operator+=(Square &sq, Direction d) { return sq = sq + d; }
// Add direction to square
constexpr Square &operator-=(Square &sq, Direction d) { return sq = sq - d; }
// File Add operator
constexpr File operator+(File f1, File f2) { return static_cast<File>(static_cast<int>(f1) + static_cast<int>(f2)); }
// File Subtract operator
constexpr File operator-(File f1, File f2) { return static_cast<File>(static_cast<int>(f1) - static_cast<int>(f2)); }
// Increment File operator
constexpr File &operator++(File &d) { return d = static_cast<File>(d + 1); }
// Decrement File operator
constexpr File &operator--(File &d) { return d = static_cast<File>(d - 1); }
// Rank Add operator
constexpr Rank operator+(Rank r1, Rank r2) { return static_cast<Rank>(static_cast<int>(r1) + static_cast<int>(r2)); }
// Rank Subtract operator
constexpr Rank operator-(Rank r1, Rank r2) { return static_cast<Rank>(static_cast<int>(r1) - static_cast<int>(r2)); }
// Increment Rank operator
constexpr Rank &operator++(Rank &d) { return d = static_cast<Rank>(d + 1); }
// Decrement Rank operator
constexpr Rank &operator--(Rank &d) { return d = static_cast<Rank>(d - 1); }
// Piece values
constexpr PieceType &operator++(PieceType &d) { return d = static_cast<PieceType>(d + 1); }
// Unary operator direction
constexpr Direction operator-(Direction d) { return static_cast<Direction>(-static_cast<int>(d)); }
// Multiply direction
constexpr Direction operator*(Direction d, int n) { return static_cast<Direction>(static_cast<int>(d) * n); }
// Flip side
constexpr Colour operator~(Colour s) { return static_cast<Colour>(static_cast<int>(s) ^ 1); }
// Bitwise AND operation on Castling Rights
constexpr Castling operator&(Castling c1, Castling c2) { return static_cast<Castling>(static_cast<int>(c1) & static_cast<int>(c2)); }
// Bitwise OR operation on Castling Rights
constexpr Castling operator|(Castling c1, Castling c2) { return static_cast<Castling>(static_cast<int>(c1) | static_cast<int>(c2)); }
// Bitwise XOR operation on Castling Rights
constexpr Castling operator^(Castling c1, Castling c2) { return static_cast<Castling>(static_cast<int>(c1) ^ static_cast<int>(c2)); }
// Bitwise assignment AND operator on Castling Rights
constexpr Castling &operator&=(Castling &c1, Castling c2) { return c1 = c1 & c2; }
// Bitwise assignment OR operation on Castling Rights
constexpr Castling &operator|=(Castling &c1, Castling c2) { return c1 = c1 | c2; }
// Bitwise assignment XOR operation on Castling Rights
constexpr Castling &operator^=(Castling &c1, Castling c2) { return c1 = c1 ^ c2; }
// Bitwise NOT operation on Castling Rights
constexpr Castling operator~(Castling c) { return c ^ ANY_SIDE; }
// Increment Castling Rights operator
constexpr Castling &operator++(Castling &c) { return c = static_cast<Castling>(static_cast<int>(c) + 1); }

// clang-format on

/******************************************\
|==========================================|
|             Score Definition             |
|==========================================|
\******************************************/

// Score type
using Score = std::pair<Value, Value>;
// Score constructors
constexpr Score _S(Value mg, Value eg) { return std::make_pair(mg, eg); }
// Score addition
inline Score operator+(Score s1, Score s2) {
  return _S(s1.first + s2.first, s1.second + s2.second);
}
// Score subtraction
inline Score operator-(Score s1, Score s2) {
  return _S(s1.first - s2.first, s1.second - s2.second);
}
// Score multiplication
inline Score operator*(Score s, int n) { return _S(s.first * n, s.second * n); }
// Score division
inline Score operator/(Score s, int n) { return _S(s.first / n, s.second / n); }
// Score unary minus
inline Score operator-(Score s) { return _S(-s.first, -s.second); }
// Score addition assignment
inline Score &operator+=(Score &s1, Score s2) { return s1 = s1 + s2; }
// Score subtraction assignment
inline Score &operator-=(Score &s1, Score s2) { return s1 = s1 - s2; }

} // namespace Maestro

#endif // DEFS_HPP
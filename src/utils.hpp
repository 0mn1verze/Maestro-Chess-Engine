#ifndef UTILS_HPP
#pragma once
#define UTILS_HPP

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>

#include "defs.hpp"
#include "move.hpp"

namespace Maestro {

/******************************************\
|==========================================|
|      Pseudo Random Number Generator      |
|==========================================|
\******************************************/

namespace PRNG {

// Seeds for the random number generator
static Bitboard seed[2] = {0xF623FE116AC4D75CULL, 0x9DA07E6D9CD459C4ULL};
// Rotate bits left
static inline Bitboard rotl(const Bitboard x, int k) {
  return (x << k) | (x >> (64 - k));
}
// Implementation of Xorshiro128+ from
// https://prng.di.unimi.it/xoroshiro128plus.c This is a fast, high-quality PRNG
static inline Bitboard getRandomU64() {
  const Bitboard s0 = seed[0];
  Bitboard s1 = seed[1];
  const Bitboard result = s0 + s1;

  s1 ^= s0;
  seed[0] = rotl(s0, 24) ^ s1 ^ (s1 << 16);
  seed[1] = rotl(s1, 37);

  return result;
}
// Get random number of specific type
template <typename T> inline T getRandom() { return T(getRandomU64()); }

} // namespace PRNG

/******************************************\
|==========================================|
|             Type conversions             |
|==========================================|
\******************************************/

// Convert File and Rank to Square
constexpr Square toSquare(File f, Rank r) {
  return static_cast<Square>(static_cast<int>(f) + (static_cast<int>(r) << 3));
}
// Return File of Square
constexpr File fileOf(Square sq) { return static_cast<File>(sq & 7); }
// Return Rank of Square
constexpr Rank rankOf(Square sq) { return static_cast<Rank>(sq >> 3); }
// Return PieceType of Piece
constexpr PieceType pieceTypeOf(Piece p) {
  return static_cast<PieceType>(p & 7);
}
// Return Colour of Piece
constexpr Colour colourOf(Piece p) { return static_cast<Colour>(p >> 3); }
// Return Piece of Colour and PieceType
constexpr Piece toPiece(Colour c, PieceType pt) {
  return static_cast<Piece>(c << 3 | pt);
}

/******************************************\
|==========================================|
|             Type validations             |
|==========================================|
\******************************************/

template <typename T> constexpr bool isValid(T t);

// Check if a square is valid
template <> constexpr bool isValid<Square>(Square sq) {
  return sq >= A1 && sq <= H8;
}
// Check if a file is valid
template <> constexpr bool isValid<File>(File f) {
  return f >= FILE_A && f <= FILE_H;
}
// Check if a rank is valid
template <> constexpr bool isValid<Rank>(Rank r) {
  return r >= RANK_1 && r <= RANK_8;
}
// Check if a piece is valid
template <> constexpr bool isValid<Piece>(Piece p) {
  return p >= wP && p <= bK;
}
// Check if a piece type is valid
template <> constexpr bool isValid<PieceType>(PieceType pt) {
  return pt >= PAWN && pt <= KING;
}
// Check if a colour is valid
template <> constexpr bool isValid<Colour>(Colour c) {
  return c >= WHITE && c <= BLACK;
}

/******************************************\
|==========================================|
|           Calculation Helpers            |
|==========================================|
\******************************************/

// Calculate draw value
constexpr Value valueDraw(U64 nodes) { return VAL_ZERO + 1 - nodes & 0x2; }

// Calculated mated in n
constexpr Value mateIn(int ply) { return VAL_MATE - ply; }

// Calculated mate in n
constexpr Value matedIn(int ply) { return -VAL_MATE + ply; }

// Calculate stat bonus (From Ethreal)
constexpr Value statBonus(int depth) {
  return depth > 13 ? 32 : 16 * depth * depth + 128 * std::max(depth - 1, 0);
}

// Calculate score w.r.t. side to move
constexpr Score absScore(Colour c, Score s) { return c == WHITE ? s : -s; }

/******************************************\
|==========================================|
|              Board Helpers               |
|==========================================|
\******************************************/

// Flip the rank of a square
constexpr Square flipRank(Square sq) {
  return static_cast<Square>(static_cast<int>(sq) ^ static_cast<int>(A8));
}
// Flip the file of a square
constexpr Square flipFile(Square sq) {
  return static_cast<Square>(static_cast<int>(sq) ^ static_cast<int>(H1));
}
// Relative rank (rank from the perspective of the side to move)
constexpr Rank relativeRank(Colour c, Rank r) {
  return (c == WHITE ? r : RANK_8 - r);
}
// Relative rank of a square (rank from the perspective of the side to move)
constexpr Rank relativeRank(Colour c, Square sq) {
  return (c == WHITE ? rankOf(sq) : RANK_8 - rankOf(sq));
}
// Relative square (square from the perspective of the side to move)
constexpr Square relativeSquare(Colour c, Square sq) {
  return static_cast<Square>(c == WHITE ? sq : flipRank(sq));
}
// Rank distance between two squares
constexpr int rankDist(Square sq1, Square sq2) {
  return static_cast<Rank>(std::abs(rankOf(sq1) - rankOf(sq2)));
}
// File distance between two squares
constexpr int fileDist(Square sq1, Square sq2) {
  return static_cast<File>(std::abs(fileOf(sq1) - fileOf(sq2)));
}

constexpr int fileDistToEdge(Square sq) {
  return fileOf(sq) >= FILE_E ? FILE_H - fileOf(sq) : fileOf(sq);
}

constexpr Rank EPRank(Colour side) { return (side == WHITE) ? RANK_4 : RANK_5; }

constexpr Direction pawnPush(Colour side) { return (side == WHITE) ? N : S; }

constexpr Direction pawnLeft(Colour side) { return (side == WHITE) ? NW : SE; }

constexpr Direction pawnRight(Colour side) { return (side == WHITE) ? NE : SW; }

/******************************************\
|==========================================|
|             Input / Output               |
|==========================================|
\******************************************/

const std::string_view pieceToChar = " PNBRQK  pnbrqk ";

inline std::string piece2Str(Piece pce) {
  return std::string{pieceToChar.at(pce)};
}

// Convert a square to a string
inline std::string sq2Str(Square sq) {
  return std::string{char('a' + fileOf(sq)), char('1' + rankOf(sq))};
}

// Convert a move to a string
inline std::string move2Str(const Move &m) {
  // Coordinates of the move
  std::string move = sq2Str(m.from()) + sq2Str(m.to());
  // Add promoted piece to the move if the move is a pawn promotion
  if (m.is<PROMOTION>())
    // To print the lowercase of piece type, we turn it into a black piece
    move += pieceToChar.at(toPiece(BLACK, m.promoted()));
  if (m == Move::none())
    move = "none";
  if (m == Move::null())
    move = "null";
  return move;
}

// Convert a piece to a char
inline char piece2Char(Piece pce) { return pieceToChar.at(pce); }

// Convert string to lower case
inline std::string toLower(std::string str) {
  std::transform(str.begin(), str.end(), str.begin(),
                 [](unsigned char c) { return std::tolower(c); });
  return str;
}

// Compare string (Case insensitive)
inline bool compareStr(std::string str1, std::string str2) {
  return toLower(str1) == toLower(str2);
}

// Convert score to a string (Format: {first} {second})
inline std::string score2Str(Score score) {
  return std::to_string(score.first) + " " + std::to_string(score.second);
}

/******************************************\
|==========================================|
|                   Time                   |
|==========================================|
\******************************************/

using TimePt = int64_t;

inline TimePt getTimeMs() {
  // Get time in milliseconds
  return std::chrono::duration_cast<std::chrono::milliseconds>(
             std::chrono::system_clock::now().time_since_epoch())
      .count();
}

/******************************************\
|==========================================|
|          Multi Dimensional Array         |
|==========================================|
\******************************************/

template <typename T, size_t N, size_t... M>
struct Array : public std::array<Array<T, M...>, N> {
  using SubArray = Array<T, N, M...>;

  void fill(const T &value) {
    T *begin = reinterpret_cast<T *>(this);
    std::fill(begin, begin + sizeof(*this) / sizeof(T), value);
  }
};

template <typename T, size_t N> struct Array<T, N> : public std::array<T, N> {};

/******************************************\
|==========================================|
|               Hash Table                 |
|==========================================|
\******************************************/

template <class Entry, int Size> class HashTable {
public:
  Entry *operator[](Key key) { return &table[key % Size]; }

private:
  std::vector<Entry> table = std::vector<Entry>(Size);
};

/******************************************\
|==========================================|
|          Distance Lookup Table           |
|                                          |
| dist[sq1][sq2] = max(rankDist, fileDist) |
|==========================================|
\******************************************/

extern Array<int, SQ_N, SQ_N> dist;

// Distance between two squares (dist = max(rankDist, fileDist))
constexpr int distance(Square sq1, Square sq2) { return dist[sq1][sq2]; }

// Check if shift is valid
constexpr bool isShift(Square from, Square to) {
  return distance(from, to) <= 3;
}

/******************************************\
|==========================================|
|                 Utility                  |
|==========================================|
\******************************************/

namespace Utility {

template <typename T, typename Pred>
void moveToFront(std::vector<T> &vec, Pred pred) {
  auto it = std::stable_partition(vec.begin(), vec.end(), pred);
  if (it != vec.end())
    std::rotate(vec.begin(), it, vec.end());
}

template <typename T> T clamp(T val, T min, T max) {
  return std::max(min, std::min(val, max));
}

} // namespace Utility

} // namespace Maestro

#endif // UTILS_HPP
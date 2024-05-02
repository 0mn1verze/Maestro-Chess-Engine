#ifndef MISC_HPP
#define MISC_HPP

#include <chrono>
#include <cstdint>
#include <format>
#include <string>

#include "defs.hpp"

/******************************************\
|==========================================|
|      Pseudo Random Number Generator      |
|==========================================|
\******************************************/

// Define U64 type
using U64 = uint64_t;

// Seeds for the random number generator
static U64 seed[2] = {0xF623FE116AC4D75CULL, 0x9DA07E6D9CD459C4ULL};

// Rotate bits left
static inline U64 rotl(const U64 x, int k) {
  return (x << k) | (x >> (64 - k));
}

// Implementation of Xorshiro128+ from
// https://prng.di.unimi.it/xoroshiro128plus.c This is a fast, high-quality PRNG
static inline U64 getRandomU64() {
  const U64 s0 = seed[0];
  U64 s1 = seed[1];
  const U64 result = s0 + s1;

  s1 ^= s0;
  seed[0] = rotl(s0, 24) ^ s1 ^ (s1 << 16);
  seed[1] = rotl(s1, 37);

  return result;
}

// Get random number of specific type
template <typename T> inline T getRandom() { return T(getRandomU64()); }

template <typename T> inline T getSparseRandom() {
  return T(getRandomU64() & getRandomU64() & getRandomU64());
}

/******************************************\
|==========================================|
|             Input and Output             |
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
  if (m.isPromotion())
    // To print the lowercase of piece type, we turn it into a black piece
    move += pieceToChar.at(toPiece(BLACK, m.promoted()));
  if (m == Move::none())
    move = "none";
  if (m == Move::null())
    move = "null";
  return move;
}

// Convert score to string
inline std::string score2Str(const Value score) {
  if (score > -VAL_MATE and score < -VAL_MATE_BOUND) {
    return std::format("mate {}", -(score + VAL_MATE) / 2 - 1);
  } else if (score < VAL_MATE and score > VAL_MATE_BOUND) {
    return std::format("mate {}", (VAL_MATE - score) / 2 + 1);
  }
  return std::format("cp {}", score / 2);
}

// Convert a piece to a char
inline char piece2Char(Piece pce) { return pieceToChar.at(pce); }

/******************************************\
|==========================================|
|                   Time                   |
|==========================================|
\******************************************/

inline Time getTimeMs() {
  // Get time in milliseconds
  return std::chrono::duration_cast<std::chrono::milliseconds>(
             std::chrono::system_clock::now().time_since_epoch())
      .count();
}

#endif // MISC_HPP
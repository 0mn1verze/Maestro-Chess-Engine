#ifndef HASH_HPP
#define HASH_HPP

#include "defs.hpp"

/******************************************\
|==========================================|
|             Zobrist Hashing              |
|==========================================|
\******************************************/

// https://www.chessprogramming.org/Zobrist_Hashing

namespace Zobrist {
extern Key pieceSquareKeys[PIECE_N][SQ_N];
extern Key enPassantKeys[FILE_N];
extern Key castlingKeys[CASTLING_N];
extern Key sideKey;
} // namespace Zobrist

// Init zobrist hashing
void initZobrist();

#endif // HASH_HPP

#include "hash.hpp"
#include "defs.hpp"
#include "utils.hpp"

/******************************************\
|==========================================|
|             Zobrist Hashing              |
|==========================================|
\******************************************/

// https://www.chessprogramming.org/Zobrist_Hashing

namespace Zobrist {
Key pieceSquareKeys[PIECE_N][SQ_N]{};
Key enPassantKeys[FILE_N]{};
Key castlingKeys[CASTLING_N]{};
Key sideKey{};
} // namespace Zobrist

// Initialize zobrist keys
void initZobrist() {
  // Initialize piece square keys
  for (Piece pce : {wP, wN, wB, wR, wQ, wK, bP, bN, bB, bR, bR, bQ, bK})
    for (Square sq = A1; sq <= H8; ++sq)
      Zobrist::pieceSquareKeys[pce][sq] = PRNG::getRandom<Key>();

  // Initialize enpassant keys
  for (File file = FILE_A; file <= FILE_H; ++file)
    Zobrist::enPassantKeys[file] = PRNG::getRandom<Key>();

  // Initialize castling keys
  for (Castling c = NO_CASTLE; c <= ANY_SIDE; ++c)
    Zobrist::castlingKeys[c] = PRNG::getRandom<Key>();

  // Initialize side key
  Zobrist::sideKey = PRNG::getRandom<Key>();
}
#ifndef HASH_HPP
#define HASH_HPP

#include "defs.hpp"
#include "move.hpp"

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

/******************************************\
|==========================================|
|        Transposition Table Entry         |
|==========================================|
| Key: 16 Bits                             |
| Move: 16 Bits                            |
| Value: 16 Bits                           |
| Eval: 16 Bits                            |
| Depth: 8 Bits                            |
| Generation: 5 Bits                       |
| PV Node: 1 Bit                           |
| Flag: 2 Bits                             |
|==========================================|
\******************************************/

enum TTFlag {
  BOUND_NONE = 0,
  BOUND_ALPHA = 1,
  BOUND_BETA = 2,
  BOUND_EXACT = 3,

  TT_BOUND_MASK = 0x03,
  TT_PV_MASK = 0x04,
  TT_GEN_MASK = 0xF8,
  TT_BUCKET_N = 3,
};

constexpr int DEPTH_OFFSET = -6;

struct TTEntry {
  U16 key() const { return key16; }
  Move move() const { return move16; }
  I16 value() const { return value16; }
  I16 eval() const { return eval16; }
  U8 depth() const { return depth8; }
  bool isPV() const { return genFlag8 & TT_PV_MASK; }
  TTFlag flag() const { return TTFlag(genFlag8 & TT_BOUND_MASK); }
  U8 gen8() const { return genFlag8 & TT_GEN_MASK; }
  void save(Key k, I16 v, bool pv, TTFlag f, U8 d, Move m, I16 ev, U8 gen8);
  U8 relativeAge(U8 gen8) const;
  bool isOccupied() const;

  // Constructors
  TTEntry() = default;
  TTEntry(const TTEntry &) = default;

private:
  // Key
  U16 key16;
  Move move16;
  I16 value16;
  I16 eval16;
  U8 depth8;
  U8 genFlag8;
};

struct TTWriter {
public:
  void write(Key k, I16 v, bool pv, TTFlag f, U8 d, Move m, I16 ev, U8 gen8);
  TTWriter(TTEntry *e) : entry(e) {}

private:
  friend class TranspositionTable;
  TTEntry *entry;
};

// Transposition Table

// - Contains 2^n buckets and each bucket contains TT_BUCKET_N entries.
// - Each entry contains information for a unique position.

class TTable {
  struct Bucket {
    TTEntry entries[TT_BUCKET_N];
    char padding[2];
  };

public:
  ~TTable() { delete[] buckets; }
  // Increment generation (last 5 bits of genFlag8)
  void newSearch() { gen8 += 8; }
  // Probe the transposition table
  std::tuple<bool, TTEntry, TTWriter> probe(Key key) const;
  // Estimate the utilization of the transposition table
  int hashFull() const;
  // Resize the transposition table
  void resize(size_t mb);
  // Clear the transposition table
  void clear();
  // Get first entry based on hash key
  TTEntry *firstEntry(const Key key) const;

private:
  friend struct TTEntry;

  size_t bucketCount;
  Bucket *buckets;
  U64 hashMask;
  U8 gen8;
};

extern TTable TT;

#endif // HASH_HPP

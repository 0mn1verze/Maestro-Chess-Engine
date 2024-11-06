#ifndef HASH_HPP
#define HASH_HPP

#include "defs.hpp"
#include "move.hpp"
#include "thread.hpp"

namespace Maestro {

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
  FLAG_NONE = 0,
  FLAG_UPPER = 1,
  FLAG_LOWER = 2,
  FLAG_EXACT = 3,

  TT_FLAG_MASK = 0x03,
  TT_PV_MASK = 0x04,
  TT_GEN_MASK = 0xF8,
  TT_BUCKET_N = 3,
};

struct TTData {
  Move move;
  Value value, eval;
  Depth depth;
  TTFlag flag;
  bool isPV;
};

struct TTEntry {

  TTData read() const {
    return {move(), value(), eval(), depth(), flag(), isPV()};
  }

  U16 key() const { return key16; }
  Move move() const { return move16; }
  Value value() const { return value16; }
  Value eval() const { return eval16; }
  Depth depth() const { return depth8 + DEPTH_ENTRY_OFFSET; }
  bool isPV() const { return genFlag8 & TT_PV_MASK; }
  TTFlag flag() const { return TTFlag(genFlag8 & TT_FLAG_MASK); }
  U8 gen8() const { return genFlag8 & TT_GEN_MASK; }
  void save(Key k, I16 v, bool pv, TTFlag f, Depth d, Move m, I16 ev, U8 gen8);
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
  void write(Key k, I16 v, bool pv, TTFlag f, Depth d, Move m, I16 ev, U8 gen8);
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
  std::tuple<bool, TTData, TTWriter> probe(Key key) const;
  // Estimate the utilization of the transposition table
  int hashFull(int maxAge = 0) const;
  // Resize the transposition table
  void resize(size_t mb, ThreadPool &);
  // Clear the transposition table
  void clear(ThreadPool &);
  // Get first entry based on hash key
  TTEntry *firstEntry(const Key key) const;
  // Prefetch entry
  static void prefetch(const void *addr);

  // Value conversions
  static Value valueToTT(Value v, int ply);
  static Value valueFromTT(Value v, int ply, int r50c);

  U8 gen8;

private:
  friend struct TTEntry;

  size_t bucketCount;
  Bucket *buckets;
  Key hashMask = 0ULL;
};

} // namespace Maestro

#endif // HASH_HPP

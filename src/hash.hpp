#ifndef HASH_HPP
#pragma once
#define HASH_HPP

#include <array>
#include <vector>

#include "defs.hpp"
#include "move.hpp"

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

void init();

} // namespace Zobrist

class ThreadPool;

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

// Transposition table entry data store (Interface)
struct TTData {
  Move move;
  Value value, eval;
  Depth depth;
  TTFlag flag;
  bool isPV;
};

struct TTEntry;

// Transposition table entry writer (Interface)
struct TTWriter {
public:
  void write(Key k, I16 v, bool pv, TTFlag f, Depth d, Move m, I16 ev, U8 gen8);
  TTWriter(TTEntry *e) : entry(e) {}

private:
  friend class TTable;
  TTEntry *entry;
};

/******************************************\
|==========================================|
|           Transposition Entry            |
|==========================================|
\******************************************/

// Transposition table entry
struct TTEntry {

  TTData read() const {
    return {move(), value(), eval(), depth(), flag(), isPV()};
  }

  // Getter functions
  U16 key() const { return _key; }
  Move move() const { return _move; }
  Value value() const { return _value; }
  Value eval() const { return _eval; }
  Depth depth() const { return _depth + DEPTH_ENTRY_OFFSET; }
  bool isPV() const { return _genFlag & TT_PV_MASK; }
  TTFlag flag() const { return TTFlag(_genFlag & TT_FLAG_MASK); }
  U8 gen8() const { return _genFlag & TT_GEN_MASK; }

  // Save entry (key, value, is pv, flag, depth, move, static eval, gen8)
  void save(Key k, I16 v, bool pv, TTFlag f, Depth d, Move m, I16 ev, U8 gen8);

  // Get relative age of entry
  U8 relativeAge(U8 gen8) const;

  // Check if entry is valid
  bool isOccupied() const;

private:
  // Key
  U16 _key;
  Move _move;
  I16 _value;
  I16 _eval;
  U8 _depth;
  U8 _genFlag;
};

/******************************************\
|==========================================|
|           Transposition Table            |
|==========================================|
\******************************************/

class TTable {
  struct Bucket {
    std::array<TTEntry, TT_BUCKET_N> entries;
    char padding[2]; // Padding for alignment
  };

public:
  // Increment generation (last 5 bits of genFlag8)
  void newSearch() { _gen += 8; }
  // Probe the transposition table
  std::tuple<bool, TTData, TTWriter> probe(Key key);
  // Estimate the utilization of the transposition table
  int hashFull(int maxAge = 0) const;
  // Resize the transposition table
  void resize(size_t mb, ThreadPool &);
  // Return size of transposition table
  size_t size() const { return _mb; }
  // Clear the transposition table
  void clear(ThreadPool &);
  // Get first entry based on hash key
  TTEntry *firstEntry(const Key key);
  // Prefetch entry
  static void prefetch(const void *addr);
  // Value conversions
  static Value valueToTT(Value v, int ply);
  static Value valueFromTT(Value v, int ply, int r50c);

  U8 _gen;

private:
  size_t _count = 0;
  size_t _mb = 0;
  std::vector<Bucket> _buckets;
  Key _hashMask = 0ULL;
};

// Init zobrist hashing

} // namespace Maestro

#endif // HASH_HPP

#include <iostream>
#include <string>

#include "defs.hpp"
#include "hash.hpp"
#include "thread.hpp"
#include "utils.hpp"

namespace Maestro {

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

/******************************************\
|==========================================|
|               Memory utils               |
|==========================================|
\******************************************/

void TTable::prefetch(const void *addr) { __builtin_prefetch(addr); }

/******************************************\
|==========================================|
|           Transposition Table            |
|==========================================|
\******************************************/

TTable TT;

void TTEntry::save(Key k, I16 v, bool pv, TTFlag f, Depth d, Move m, I16 ev,
                   U8 gen8) {

  const U16 k16 = k >> 48;

  // Don't overwrite move if we don't have a new one and the position is the
  // same
  if (m || k16 != key16)
    move16 = m;

  // Don't overwrite an entry with the same position, unless we have an exact
  // bound or depth is nearly as good as the old one
  if (flag() != BOUND_EXACT && k16 == key16 && d < depth8 - 2)
    return;

  // Overwrite less valuable entries
  key16 = k16;
  value16 = v;
  eval16 = ev;
  genFlag8 = U8(gen8 | (pv << 2) | f);
  depth8 = U8(d - DEPTH_OFFSET);
}

U8 TTEntry::relativeAge(U8 gen8) const {
  return (255 + 8 + gen8 - genFlag8) & TT_GEN_MASK;
}

bool TTEntry::isOccupied() const { return bool(depth8); }

void TTWriter::write(Key k, I16 v, bool pv, TTFlag f, Depth d, Move m, I16 ev,
                     U8 gen8) {
  entry->save(k, v, pv, f, d, m, ev, gen8);
}

// Get first entry based on hash key
TTEntry *TTable::firstEntry(const Key key) const {
  return &buckets[key & hashMask].entries[0];
}

// Probe the transposition table
std::tuple<bool, TTData, TTWriter> TTable::probe(Key key) const {
  // Get entry
  TTEntry *entry = firstEntry(key);
  // Calculate truncated key
  const U16 k16 = key >> 48;

  for (int i = 0; i < TT_BUCKET_N; ++i)
    if (entry[i].key() == k16)
      return {entry[i].isOccupied(), entry[i].read(), TTWriter(&entry[i])};

  // Find an entry to be replaced according to the replacement strategy
  TTEntry *replace = entry;
  for (int i = 1; i < TT_BUCKET_N; ++i)
    if (replace->depth() - replace->relativeAge(gen8) * 2 >
        entry[i].depth() - entry[i].relativeAge(gen8) * 2)
      replace = &entry[i];

  return {false, TTData(), TTWriter(replace)};
}
// Estimate the utilization of the transposition table
int TTable::hashFull(int maxAge) const {
  int cnt = 0;
  for (size_t i = 0; i < 1000; ++i) {
    for (size_t j = 0; j < bucketCount; ++j)
      if (buckets[i].entries[j].isOccupied()) {
        int age = (gen8 >> 3) - (buckets[i].entries[j].gen8() >> 3);
        if (age < 0)
          age += 1 << 5;
        cnt += age <= maxAge;
      }
  }

  return cnt / bucketCount;
}
// Resize the transposition table
void TTable::resize(size_t mb, ThreadPool &threads) {
  constexpr size_t MB = 1ULL << 20;
  U64 keySize = 16ULL;

  if (hashMask)
    delete[] buckets;

  while ((1ULL << keySize) * sizeof(Bucket) < mb * MB / 2)
    ++keySize;

  bucketCount = (1ULL << keySize);

  buckets = new Bucket[bucketCount];

  hashMask = bucketCount - 1;

  clear(threads);
}
// Clear the transposition table
void TTable::clear(ThreadPool &threads) {
  gen8 = 0;
  const size_t n = threads.size();

  for (size_t i = 0; i < n; ++i) {
    threads.startCustomJob(i, [this, i, n] {
      const size_t stride = bucketCount / n;
      const size_t begin = i * stride;
      const size_t len = (i + 1 == n) ? bucketCount - begin : stride;

      std::fill_n(buckets + begin, len, Bucket{});
    });
  }

  threads.waitForThreads();
}

// Value conversions
Value TTable::valueToTT(Value v, int ply) {
  return v >= VAL_MATE_BOUND ? v + ply : v <= -VAL_MATE_BOUND ? v - ply : v;
}
Value TTable::valueFromTT(Value v, int ply, int r50c) {
  if (v == VAL_NONE)
    return VAL_NONE;

  if (v >= VAL_MATE_BOUND) {
    // Downgrade potential false mate score (Too close to 50 move draw)
    if (v >= VAL_MATE_BOUND and VAL_MATE - v > 100 - r50c)
      return VAL_MATE_BOUND - 1;
    return v - ply;
  }

  if (v <= -VAL_MATE_BOUND) {
    // Downgrade potential false mate score (Too close to 50 move draw)
    if (v <= -VAL_MATE_BOUND and VAL_MATE + v > 100 - r50c)
      return -VAL_MATE_BOUND + 1;
    return v + ply;
  }

  return v;
}

} // namespace Maestro
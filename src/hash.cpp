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

// Initialize zobrist keys
void init() {

  PRNG rng;

  // Initialize piece square keys
  for (Piece pce : {wP, wN, wB, wR, wQ, wK, bP, bN, bB, bR, bR, bQ, bK})
    for (Square sq = A1; sq <= H8; ++sq)
      Zobrist::pieceSquareKeys[pce][sq] = rng.getRandom<Key>();

  // Initialize enpassant keys
  for (File file = FILE_A; file <= FILE_H; ++file)
    Zobrist::enPassantKeys[file] = rng.getRandom<Key>();

  // Initialize castling keys
  for (Castling c = NO_CASTLE; c <= ANY_SIDE; ++c)
    Zobrist::castlingKeys[c] = rng.getRandom<Key>();

  // Initialize side key
  Zobrist::sideKey = rng.getRandom<Key>();
}

} // namespace Zobrist

/******************************************\
|==========================================|
|               Memory utils               |
|==========================================|
\******************************************/

void TTable::prefetch(const void *addr) { __builtin_prefetch(addr); }

/******************************************\
|==========================================|
|           Interface functions            |
|==========================================|
\******************************************/

void TTWriter::write(Key k, I16 v, bool pv, TTFlag f, Depth d, Move m, I16 ev,
                     U8 gen8) {
  entry->save(k, v, pv, f, d, m, ev, gen8);
}

/******************************************\
|==========================================|
|               Save Entry                 |
|==========================================|
\******************************************/

void TTEntry::save(Key k, I16 v, bool pv, TTFlag f, Depth d, Move m, I16 ev,
                   U8 gen8) {
  const U16 k16 = k >> 48;

  // Don't overwrite move if we don't have a new one and the position is the
  // same
  if (m || k16 != _key)
    _move = m;

  // Don't overwrite an entry with the same position, unless we have an exact
  // bound or depth is nearly as good as the old one
  if (flag() != FLAG_EXACT && k16 == _key &&
      d - DEPTH_ENTRY_OFFSET + 2 * pv < _depth - 4 && relativeAge(gen8))
    return;

  // Overwrite less valuable entries
  _key = k16;
  _value = v;
  _eval = ev;
  _genFlag = U8(gen8 | (U8(pv) << 2) | f);
  _depth = U8(d - DEPTH_ENTRY_OFFSET);
}

/******************************************\
|==========================================|
|               Probe table                |
|==========================================|
\******************************************/

// Probe the transposition table
std::tuple<bool, TTData, TTWriter> TTable::probe(Key key) {
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
    if (replace->depth() - replace->relativeAge(_gen) * 2 >
        entry[i].depth() - entry[i].relativeAge(_gen) * 2)
      replace = &entry[i];

  return {false, TTData(), TTWriter(replace)};
}

/******************************************\
|==========================================|
|             Helper functions             |
|==========================================|
\******************************************/

U8 TTEntry::relativeAge(U8 gen8) const {
  return (255 + 8 + gen8 - _genFlag) & TT_GEN_MASK;
}

bool TTEntry::isOccupied() const { return bool(_depth); }

// Get first entry based on hash key
TTEntry *TTable::firstEntry(const Key key) {
  return &_buckets[key & _hashMask].entries[0];
}

// Estimate the utilization of the transposition table
int TTable::hashFull(int maxAge) const {
  int cnt = 0;
  for (size_t i = 0; i < 1000; ++i) {
    for (size_t j = 0; j < TT_BUCKET_N; ++j)
      if (_buckets[i].entries[j].isOccupied()) {
        int age = (_gen >> 3) - (_buckets[i].entries[j].gen8() >> 3);
        if (age < 0)
          age += 1 << 5;
        cnt += age <= maxAge;
      }
  }

  return cnt / TT_BUCKET_N;
}
// Resize the transposition table
void TTable::resize(size_t mb, ThreadPool &threads) {
  constexpr size_t MB = 1ULL << 20;
  U64 keySize = 16ULL;

  if (_buckets.capacity() > 0) {
    _buckets.clear();
    _buckets.shrink_to_fit();
  }

  while ((1ULL << keySize) * sizeof(Bucket) <= mb * MB / 2)
    ++keySize;

  _count = (1ULL << keySize);

  std::cout << "info string Resizing hash table to " << _count << " entries"
            << std::endl;

  std::cout << "info string Hash table size: " << _count * sizeof(Bucket) / MB
            << " MB" << std::endl;

  std::cout << "info string Hash table bucket size: " << sizeof(Bucket)
            << " Bytes" << std::endl;

  _buckets.reserve(_count);

  _hashMask = _count - 1;

  _mb = mb;

  clear(threads);
}

// Clear the transposition table
void TTable::clear(ThreadPool &threads) {
  _gen = 0;
  const size_t n = threads.size();

  for (size_t i = 0; i < n; ++i) {
    threads.startJob(i, [this, i, n] {
      const size_t stride = _count / n;
      const size_t begin = i * stride;
      const size_t len = (i + 1 == n) ? _count - begin : stride;

      std::fill_n(_buckets.begin() + begin, len, Bucket{});
    });
  }

  threads.main()->waitForThread();
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
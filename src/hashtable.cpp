#include <algorithm>
#include <cstdint>
#include <iostream>
#include <iterator>

#include "defs.hpp"
#include "hashtable.hpp"

TTTable hashTable; // Global hash table

// Mate scores have to be adjusted relative to the root so we can know if a mate
// is faster or slower
static Value TTValueFrom(Value value, int ply) {
  return value == VAL_NONE         ? VAL_NONE
         : value > VAL_MATE_BOUND  ? value - ply
         : value < -VAL_MATE_BOUND ? value + ply
                                   : value;
}

static Value TTValueTo(Value value, int ply) {
  return value == VAL_NONE         ? VAL_NONE
         : value > VAL_MATE_BOUND  ? value + ply
         : value < -VAL_MATE_BOUND ? value - ply
                                   : value;
}

// Update table generation
void TTUpdate() {
  hashTable.generation += TT_MASK_BOUND + 1;
  std::cout << hashTable.generation << std::endl;
}

// Prefetch the hash entry to make memory access quicker
void TTPrefetch(const Key key) {
  __builtin_prefetch(&hashTable.buckets[key & hashTable.hashMask]);
}

// Init hash table
Count TTInit(int mb) {
  const std::uint64_t MB = 1ULL << 20;
  std::uint64_t keySize = 16ULL;

  if (hashTable.hashMask) {
    delete[] hashTable.buckets;
  }

  while ((1ULL << keySize) * sizeof(TTBucket) <= mb * MB / 2)
    keySize++;

  hashTable.buckets = new TTBucket[1ULL << keySize];

  hashTable.hashMask = (1ULL << keySize) - 1ULL;

  TTClear();

  return ((hashTable.hashMask + 1) * sizeof(TTBucket)) / MB;
}

// Estimate the permill of the table being used
Count TTHashFull() {
  Count used = 0;

  for (int i = 0; i < 1000; i++) {
    for (int j = 0; j < TT_BUCKET_NB; j++) {
      used += ((hashTable.buckets[i].entries[j].generation & TT_MASK_BOUND) !=
               HashNone) &&
              ((hashTable.buckets[i].entries[j].generation & TT_MASK_AGE) ==
               hashTable.generation);
    }
  }

  return used / TT_BUCKET_NB;
}

// Probe hash table
bool TTProbe(const Key key, int ply, Move &move, Value &value, Value &eval,
             Depth &depth, HashFlag &flag) {
  const std::uint16_t hash16 = key >> 48;
  TTEntry *slots = hashTable.buckets[key & hashTable.hashMask].entries;

  for (int i = 0; i < TT_BUCKET_NB; i++) {
    if (slots[i].hash16 == hash16) {

      // Update the generation of the hash entry while preserving the hash flag
      slots[i].generation =
          hashTable.generation | (slots[i].generation & TT_MASK_BOUND);

      move = slots[i].move;
      value = TTValueFrom(slots[i].value, ply);
      eval = slots[i].eval;
      depth = slots[i].depth;
      flag = HashFlag(slots[i].generation & TT_MASK_BOUND);
      return true;
    }
  }

  return false;
}

void TTStore(const Key key, int ply, Move move, Value value, Value eval,
             Depth depth, HashFlag flag) {
  int i;
  const std::uint16_t hash16 = key >> 48;
  TTEntry *slots = hashTable.buckets[key & hashTable.hashMask].entries;
  TTEntry *replace = slots; // pointer to first element of slots

  // Find a matching hash and moving the older hash records up the bucket list
  for (i = 0; i < TT_BUCKET_NB && slots[i].hash16 != hash16; i++) {
    std::uint8_t slotAge =
        slots[i].depth -
        ((259 + hashTable.generation - slots[i].generation) & TT_MASK_AGE);
    std::uint8_t repAge =
        replace->depth -
        ((259 + hashTable.generation - replace->generation) & TT_MASK_AGE);

    if (repAge >= slotAge)
      replace = &slots[i];
  }

  replace = (i != TT_BUCKET_NB) ? &slots[i] : replace;

  if (flag != HashExact and hash16 == replace->hash16 and
      depth < replace->depth) {
    return;
  }

  if (move != Move::none() || hash16 != replace->hash16) {
    replace->move = move;
  }

  replace->depth = depth;
  replace->generation = (std::uint8_t)flag | hashTable.generation;
  replace->value = TTValueTo(value, ply);
  replace->eval = eval;
  replace->hash16 = hash16;
}

void TTClear() {
  const uint64_t MB = 1ULL << 20;
  std::fill(hashTable.buckets, hashTable.buckets + hashTable.hashMask + 1,
            TTBucket{});
}
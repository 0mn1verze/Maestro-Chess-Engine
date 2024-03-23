#pragma once

#ifndef HASHTABLE_HPP
#define HASHTABLE_HPP

#include <vector>

#include "defs.hpp"
#include "position.hpp"

// Following the example of Ethreal

enum HashFlag {
  HashNone,
  HashExact,
  HashAlpha,
  HashBeta,
  TT_MASK_BOUND = 0x03,
  TT_MASK_AGE = 0xFC,
  TT_BUCKET_NB = 3
};

struct TTEntry {
  Value eval, value;
  Depth depth;
  Move move;
  std::uint16_t hash16;
  std::uint8_t generation;
};

struct TTBucket {
  TTEntry entries[TT_BUCKET_NB];
  std::uint16_t padding;
};

struct TTTable {
  TTBucket *buckets;
  std::uint64_t hashMask;
  std::uint8_t generation;
};

// Hash table
void TTUpdate();
// Prefetch the entry for the given key
void TTPrefetch(const Key key);
// Init hash table and return the actual size allocated
Count TTInit(int mb);
// Get hash table statistics
Count TTHashFull();
// Probe the hash table
bool TTProbe(const Key key, int ply, Move &move, Value &value, Value &eval,
             Depth &depth, HashFlag &flag);
// Store the hash table entry
void TTStore(const Key key, int ply, Move move, Value value, Value eval,
             Depth depth, HashFlag flag);
// Clear the hash table
void TTClear();

#endif // HASHTABLE_HPP
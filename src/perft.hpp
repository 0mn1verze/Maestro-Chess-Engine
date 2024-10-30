#ifndef PERFT_HPP
#define PERFT_HPP

#include "defs.hpp"
#include "position.hpp"
#include "utils.hpp"


// Function to count the number of leaf nodes at a given depth
Count perftDriver(Position &pos, int depth);
// Function to count the number of leaf nodes at a given depth (With Debugging
// and Performance Information)
void perftTest(Position &pos, int depth);
// Function to test multiple position from bench.csv
void perftBench();

struct PerftPosition {
  std::string fen;
  int depth;
  Count nodes;
};

#endif // PERFT_HPP
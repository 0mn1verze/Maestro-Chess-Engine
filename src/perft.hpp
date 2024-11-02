#ifndef PERFT_HPP
#define PERFT_HPP

#include <vector>

#include "defs.hpp"
#include "position.hpp"
#include "thread.hpp"
#include "utils.hpp"

namespace Maestro {

struct PerftPosition {
  std::string fen;
  int depth;
  U32 nodes;
};

// Function to count the number of leaf nodes at a given depth (With Debugging
// and Performance Information)
void perftTest(Position &pos, int depth);
// Function to test multiple position from bench.csv
void perftBench(ThreadPool &threads, std::string filePath);

} // namespace Maestro

#endif // PERFT_HPP
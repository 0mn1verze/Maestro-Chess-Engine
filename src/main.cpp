#include <cstring>
#include <iostream>

#include "bitboard.hpp"
#include "defs.hpp"
#include "hash.hpp"
#include "movegen.hpp"
#include "perft.hpp"
#include "position.hpp"
#include "thread.hpp"

int main() {

  initBitboards();

  Threads.set(10);

  TT.resize(100);

  TT.resize(50);

  Threads.set(30);

  perftBench();

  // Position pos;
  // BoardState st{};
  // const std::string fen(startPos);
  // pos.set(fen, st);

  // perftTest(pos, 7);

  return 0;
}
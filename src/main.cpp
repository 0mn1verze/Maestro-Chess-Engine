#include <cstring>
#include <iostream>

#include "bitboard.hpp"
#include "defs.hpp"
#include "movegen.hpp"
#include "perft.hpp"
#include "position.hpp"

int main() {

  initBitboards();

  perftBench();

  // Position pos;
  // BoardState st{};
  // const std::string fen(startPos);
  // pos.set(fen, st);

  // perftTest(pos, 7);

  return 0;
}
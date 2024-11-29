#include <cstring>
#include <iostream>

#include "perft.hpp"
#include "polyglot.hpp"
#include "uci.hpp"

using namespace Maestro;

int main() {

  UCI uci;

  uci.loop();

  // std::istringstream cmd1(
  //     "fen r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0
  //     " "1 ");
  // std::istringstream cmd2("perft 6");

  // uci.pos(cmd1);

  // uci.go(cmd2);

  // initBitboards();
  // Zobrist::init();

  // Position pos;
  // BoardState st{};
  // const std::string fen(startPos);
  // pos.set(fen, st);

  // perftTest(pos, 7);

  // perftBench("bench.csv");

  return 0;
}
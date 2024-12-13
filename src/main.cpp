#include <cstring>
#include <iostream>

#include "movepicker.hpp"
#include "perft.hpp"
#include "uci.hpp"

using namespace Maestro;

int main() {

  UCI uci;

  // uci.loop();

  std::istringstream pos(
      "position fen r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w "
      "KQkq - 0 1 ");
  uci.pos(pos);

  std::istringstream go("go depth 20");
  uci.go(go);

  return 0;
}
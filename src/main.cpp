#include <cstring>
#include <iostream>

#include "bitboard.hpp"
#include "defs.hpp"
#include "hash.hpp"
#include "movegen.hpp"
#include "perft.hpp"
#include "position.hpp"
#include "thread.hpp"
#include "uci.hpp"

int main() {

  UCI uci;

  uci.loop();

  // Position pos;
  // BoardState st{};
  // const std::string fen(startPos);
  // pos.set(fen, st);

  // perftTest(pos, 7);

  return 0;
}
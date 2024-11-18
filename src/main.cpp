#include <cstring>
#include <iostream>

#include "bitboard.hpp"
#include "defs.hpp"
#include "hash.hpp"
#include "movegen.hpp"
#include "movepick.hpp"
#include "perft.hpp"
#include "position.hpp"
#include "thread.hpp"
#include "uci.hpp"

using namespace Maestro;

int main() {

  UCI uci;

  uci.loop();

  // Limits limits;

  // limits.depth = 8;

  // uci.engine.perft(limits);

  return 0;
}
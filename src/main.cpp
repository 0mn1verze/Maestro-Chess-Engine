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

int main() {

  // UCI uci;

  // uci.loop();

  ThreadPool threads;

  threads.set(1);

  Position pos;
  BoardState st{};

  pos.set(startPos.data(), st, threads.main());

  

  // Position pos;
  // BoardState st{};
  // const std::string fen(startPos);
  // pos.set(fen, st);

  // perftTest(pos, 7);

  return 0;
}
#include <cstring>
#include <iostream>

#include "history.hpp"
#include "movepicker.hpp"
#include "perft.hpp"
#include "polyglot.hpp"
#include "uci.hpp"

using namespace Maestro;

int main() {

  UCI uci;

  uci.loop();

  // KillerTable kt;
  // HistoryTable ht;
  // CaptureHistoryTable cht;

  // kt.clear();

  // kt.update(Move(0), 0);

  // std::istringstream cmd1(
  //     "fen r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0
  //     " "1 ");
  // std::istringstream cmd2("go depth 6");

  // uci.pos(cmd1);

  // uci.go(cmd2);

  // initBitboards();
  // Zobrist::init();

  // Position pos;
  // BoardState st{};
  // const std::string fen(trickyPos);
  // pos.set(fen, st);

  // MovePicker mp(pos, Move::none(), 10, 0, ht, kt, cht);

  // Move move;

  // while ((move = mp.next()) != Move::none()) {
  //   std::cout << move2Str(move) << std::endl;
  // }

  // perftTest(pos, 6);

  // perftBench("bench.csv");

  return 0;
}
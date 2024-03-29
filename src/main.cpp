#include <cstring>
#include <iostream>

#include "eval.hpp"
#include "perft.hpp"
#include "polyglot.hpp"
#include "position.hpp"
#include "uci.hpp"

int main() {

  UCI uci;

  uci.loop();

  // Position pos;
  // StateList states{new std::deque<BoardState>(1)};
  // std::string fen{"r3r3/5R2/PP1p2pk/3n3p/K2p4/8/7R/8 b - - 1 50"};
  // pos.set(fen, states->back());

  // std::stringstream command{"c6d4 e5d4 c2h2"};

  // Move m;
  // std::string token;
  // while (command >> token && (m = uci.parseMove(token, pos)) != Move::none())
  // {
  //   states->emplace_back();
  //   pos.makeMove(m, states->back());
  // }

  // pos.print();

  // states->emplace_back();
  // pos.makeNullMove(states->back());
  // pos.unmakeNullMove();

  // pos.print();

  return 0;
}
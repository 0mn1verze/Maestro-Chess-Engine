#ifndef UCI_HPP
#define UCI_HPP

#include <Thread>

#include <iostream>

#include <sstream>

#include "defs.hpp"
#include "hashtable.hpp"
#include "position.hpp"
#include "search.hpp"

// Time limits
struct Limits {
  Time start, time, inc, timeLimit;
  int depth, movesToGo;
  bool depthSet, timeSet, selfControl, infinite;
};

/******************************************\
|==========================================|
|       UCI loop and command parsing       |
|==========================================|
\******************************************/

class UCI {
public:
  UCI();
  // Main loop of the chess engine
  void loop();

  // Parse the UCI go command
  void parseGo(std::stringstream &command, Position &pos, StateList &states);
  // Parse the UCI position command
  void parsePosition(std::stringstream &command, Position &pos,
                     StateList &states);
  // Parse the UCI moves
  Move parseMove(std::string_view token, const Position &pos);
  // Parse go time controls
  void parseTime(const Limits &limits, TimeControl &time);
  // Search clear
  void searchClear();

  SearchWorker worker;
};

#endif // UCI_HPP
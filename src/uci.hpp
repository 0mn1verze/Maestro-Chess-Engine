#ifndef UCI_HPP
#define UCI_HPP

#include <cstring>

#include "defs.hpp"
#include "hash.hpp"
#include "move.hpp"
#include "position.hpp"
#include "thread.hpp"
#include "utils.hpp"

namespace Maestro {

/******************************************\
|==========================================|
|              Search Limits               |
|==========================================|
\******************************************/

struct Limits {
  TimePt time[COLOUR_N], inc[COLOUR_N], movetime, startTime;
  int movesToGo, depth;
  bool perft, infinite;
  U64 nodes;

  bool useTimeManagement() const { return time[WHITE] || time[BLACK]; }
  void trace() const;
};

/******************************************\
|==========================================|
|              Engine Config               |
|==========================================|
\******************************************/

struct Config {
  size_t hashSize;
  int threads;
  bool useBook;
};

/******************************************\
|==========================================|
|               Engine Class               |
|==========================================|
\******************************************/

class Engine {
public:
  Engine();

  void waitForSearchFinish();
  void setPosition(const std::string fen,
                   const std::vector<std::string> &moves);

  void perft(Limits &limits);
  void bench();
  void go(Limits &limits);
  void stop();

  void clear();

  // Change engine config
  void setOption(std::istringstream &is);
  void resizeThreads(int n);
  void setTTSize(size_t mb);

  std::string fen() const;
  void print() const;

private:
  Position pos;
  StateListPtr states;

  ThreadPool threads;
  TTable TT;
  Config config;
};

/******************************************\
|==========================================|
|                 UCI Class                |
|==========================================|
\******************************************/

class UCI {
public:
  // Main loop of the chess engine
  void loop();
  // Parse the UCI go command
  void go(std::istringstream &is);
  // Parse the UCI position command
  void pos(std::istringstream &is);
  // Parse move
  static Move toMove(const Position &pos, const std::string &move);
  // Parse go time controls
  Limits parseLimits(std::istringstream &is);

private:
  Engine engine;
};

} // namespace Maestro

#endif // UCI_HPP
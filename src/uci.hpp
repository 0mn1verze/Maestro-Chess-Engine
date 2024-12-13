#ifndef UCI_HPP
#pragma once
#define UCI_HPP

#include <string>

#include "defs.hpp"
#include "engine.hpp"
#include "move.hpp"
#include "position.hpp"
#include "utils.hpp"

namespace Maestro {

/******************************************\
|==========================================|
|              Engine Config               |
|==========================================|
\******************************************/

constexpr std::string_view NAME = "Maestro";
constexpr std::string_view AUTHOR = "Evan Fung";
constexpr std::string_view VERSION = "1.2";
constexpr std::string_view BOOK_FILE = "OPTIMUS2403.bin";
constexpr std::string_view BENCH_FILE = "bench.csv";

constexpr size_t HASH_SIZE = 32;
constexpr size_t THREADS = 1;
constexpr bool USE_BOOK = false;
constexpr int MOVE_OVERHEAD = 600;

/******************************************\
|==========================================|
|           Input / Output Structs         |
|==========================================|
\******************************************/

struct PrintInfo {
  Depth depth;
  Depth selDepth;
  TimePt timeMs;
  Value score;
  U64 nodes;
  U64 nps;
  std::string_view pv;
  int hashFull;
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

  // UCI command parsing

  // Parse the UCI go command
  void go(std::istringstream &is);
  // Parse the UCI position command
  void pos(std::istringstream &is);

  // UCI helper functions

  // Parse go time controls
  Limits parseLimits(std::istringstream &is);
  // Parse move
  static Move toMove(const Position &pos, const std::string &move);

  // Engine output functions

  // Print UCI info
  static void uciReport(const PrintInfo &info);
  // Print current move
  static void uciReportCurrentMove(Depth depth, Move move, int currmove);
  // Print search node count, hash full and nps
  static void uciReportNodes(ThreadPool &threads, int hashFull, TimePt elapsed);

private:
  // UCI command parsing

  // Parse the UCI setoption command
  void setOption(std::istringstream &is);

  Engine engine;
};

} // namespace Maestro

#endif // UCI_HPP
#ifndef UCI_HPP
#define UCI_HPP

#include <cstring>

#include "defs.hpp"
#include "hash.hpp"
#include "move.hpp"
#include "polyglot.hpp"
#include "position.hpp"
#include "thread.hpp"
#include "utils.hpp"

namespace Maestro {

/******************************************\
|==========================================|
|              Engine Config               |
|==========================================|
\******************************************/

constexpr std::string_view NAME = "Maestro";
constexpr std::string_view AUTHOR = "Evan Fung";
constexpr std::string_view VERSION = "2.0";
constexpr std::string_view BENCH_FILE = "bench.csv";
constexpr std::string_view BOOK_FILE = "OPTIMUS2403.bin";
constexpr std::string_view NNUE_FILE = "nn-eba324f53044.nnue";

constexpr size_t DEFAULT_HASH_SIZE = 32;
constexpr size_t DEFAULT_THREADS = 1;
constexpr bool DEFAULT_USE_BOOK = true;
constexpr int DEFAULT_MULTI_PV = 1;
constexpr int MOVE_OVERHEAD = 1000;

struct Config {
  size_t hashSize = DEFAULT_HASH_SIZE;
  int threads = DEFAULT_THREADS;
  bool useBook = DEFAULT_USE_BOOK;
  int multiPV = DEFAULT_MULTI_PV;
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
  PolyBook book;

  ThreadPool threads;
  TTable tt;
  Config config;
  SearchState searchState{config, threads, tt};
};

/******************************************\
|==========================================|
|           Input / Output Structs         |
|==========================================|
\******************************************/

struct PrintInfo {
  Depth depth;
  Depth selDepth;
  int multiPVIdx;
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
  // Parse the UCI go command
  void go(std::istringstream &is);
  // Parse the UCI position command
  void pos(std::istringstream &is);
  // Parse move
  static Move toMove(const Position &pos, const std::string &move);
  // Parse go time controls
  Limits parseLimits(std::istringstream &is);
  // Print UCI info
  static void uciReport(const PrintInfo &info);
  // Print current move
  static void uciReportCurrentMove(Depth depth, Move move, int currmove);
  // Print search node count, hash full and nps
  static void uciReportNodes(U64 nodes, int hashFull, TimePt elapsed);

private:
  Engine engine;
};

} // namespace Maestro

#endif // UCI_HPP
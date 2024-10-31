#include <iostream>
#include <sstream>

#include "bitboard.hpp"
#include "hash.hpp"
#include "perft.hpp"
#include "position.hpp"
#include "uci.hpp"

constexpr std::string_view NAME = "Maestro";
constexpr std::string_view AUTHOR = "Evan Fung";

void Limits::trace() const {
  std::cout << "time: " << time[WHITE] << " " << time[BLACK] << std::endl;
  std::cout << "inc: " << inc[WHITE] << " " << inc[BLACK] << std::endl;
  std::cout << "startTime: " << startTime << std::endl;
  std::cout << "depth: " << depth << std::endl;
  std::cout << "movestogo: " << movesToGo << std::endl;
  std::cout << "movetime: " << movetime << std::endl;
  std::cout << "infinite: " << infinite << std::endl;
  std::cout << "perft: " << perft << std::endl;
}

// Engine constructor
Engine::Engine() : states(new std::deque<BoardState>(1)) {
  // // Initialize bitboards
  // initBitboards();
  // // Initialize zobrist keys
  // initZobrist();

  // Initialise default config
  config.hashSize = 64;
  config.threads = 1;
  config.useBook = true;

  // Initialise thread pool
  threads.set(config.threads);
  // Initialise transposition table
  TT.resize(config.hashSize, threads);

  // Set starting position
  pos.set(startPos.data(), states->back(), threads.main());
}

// Wait for search to finish
void Engine::waitForSearchFinish() { threads.waitForThreads(); }

// Parse UCI move
Move UCI::toMove(const Position &pos, const std::string &move) {
  if (move == "none")
    return Move::none();
  if (move == "null")
    return Move::null();

  MoveList<ALL> moves(pos);

  for (const Move &m : moves)
    if (move == move2Str(m))
      return m;

  return Move::none();
}

// Resize threads
void Engine::resizeThreads(int n) {
  threads.set(n);
  config.threads = n;
}

void Engine::setTTSize(size_t mb) {
  TT.resize(mb, threads);
  config.hashSize = mb;
}

std::string Engine::fen() const { return pos.fen(); }

// Print engine state
void Engine::print() const { pos.print(); }

// Set position
void Engine::setPosition(const std::string fen,
                         const std::vector<std::string> &moves) {
  pos.set(fen, states->back(), threads.main());

  for (const std::string &move : moves) {
    Move m = UCI::toMove(pos, move);
    pos.makeMove(m, states->back());
  }
}

// Engine perft
void Engine::setOption(std::istringstream &is) {
  std::string token, name;
  int value;
  is >> token; // Consume name token
  is >> name;  // Get name
  is >> token; // Consume value token
  is >> value; // Get value

  if (name == "Hash") {
    setTTSize(value);
  } else if (name == "Threads") {
    resizeThreads(value);
  }
}

void Engine::perft(Limits &limits) { perftTest(pos, limits.depth); }

void Engine::bench() { perftBench(threads); }

void Engine::go(Limits &limits) {}

void Engine::stop() { threads.waitForThreads(); }

void Engine::clear() {}

// Main loop of the chess engine
void UCI::loop() {
  std::string input, token;

  do {
    std::getline(std::cin, input);

    std::istringstream is(input);

    token.clear();
    is >> std::skipws >> token;

    if (token == "uci") {
      std::cout << "id name " << NAME << std::endl;
      std::cout << "id author " << AUTHOR << std::endl;
      std::cout << "uciok" << std::endl;
    } else if (token == "isready") {
      std::cout << "readyok" << std::endl;
    } else if (token == "quit" || token == "stop") {
      //   worker.stop = true;
    } else if (token == "setoption") {
      engine.setOption(is);
    } else if (token == "ucinewgame") {
      engine.clear();
    } else if (token == "go") {
      go(is);
    } else if (token == "position") {
      pos(is);
    } else if (token == "b") {
      engine.print();
    } else if (token == "bench" || token == "test") {
      engine.bench();
    }
  } while (token != "quit");
}

// Parse UCI limits
Limits UCI::parseLimits(std::istringstream &is) {
  Limits limits{};
  std::string token;

  limits.startTime = getTimeMs();

  while (is >> token) {
    if (token == "wtime") {
      is >> limits.time[WHITE];
    } else if (token == "btime") {
      is >> limits.time[BLACK];
    } else if (token == "winc") {
      is >> limits.inc[WHITE];
    } else if (token == "binc") {
      is >> limits.inc[BLACK];
    } else if (token == "depth") {
      is >> limits.depth;
    } else if (token == "movestogo") {
      is >> limits.movesToGo;
    } else if (token == "movetime") {
      is >> limits.movetime;
    } else if (token == "infinite") {
      limits.infinite = true;
    } else if (token == "perft") {
      limits.perft = true;
    }
  }

  limits.trace();

  return limits;
}

// Parse the UCI go command
void UCI::go(std::istringstream &is) {
  Limits limits = parseLimits(is);

  if (limits.perft)
    engine.perft(limits);
  else
    engine.go(limits);
}

// Parse the UCI position command
void UCI::pos(std::istringstream &is) {
  std::string token, fen;
  is >> token;

  if (token == "startpos") {
    fen = startPos.data();
    is >> token;
  } else if (token == "fen") {
    while (is >> token && token != "moves") {
      fen += token + " ";
    }
  } else {
    return;
  }

  std::vector<std::string> moves;

  while (is >> token)
    moves.push_back(token);

  engine.setPosition(fen, moves);
}

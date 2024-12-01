#include <iostream>

#include "bitboard.hpp"
#include "defs.hpp"
#include "engine.hpp"
#include "eval.hpp"
#include "hash.hpp"
#include "movegen.hpp"
#include "nnue.hpp"
#include "perft.hpp"
#include "polyglot.hpp"
#include "position.hpp"
#include "search.hpp"
#include "uci.hpp"
#include "utils.hpp"

namespace Maestro {

// Engine constructor
Engine::Engine() : states(new std::deque<BoardState>(1)) {
  // Initialize bitboards
  Bitboards::init();
  // Initialize zobrist keys
  Zobrist::init();
  // Initialize evaluation
  Eval::initEval();
  // Initialize threads
  threads.set(THREADS, searchState);
  // Initialize transposition table
  tt.resize(HASH_SIZE, threads);
  // Set starting position
  pos.set(startPos.data(), states->back());
  // Initialize polyglot book
  if (USE_BOOK)
    book.init(BOOK_FILE);
  // Initialize nnue
  if (USE_NNUE)
    nnue_init(NNUE_FILE.data());
}

// Wait for search to finish
void Engine::waitForSearchFinish() {}

std::string Engine::fen() const { return pos.fen(); }

// Print engine state
void Engine::print() const { pos.print(); }

// Set position
void Engine::setPosition(const std::string fen,
                         const std::vector<std::string> &moves) {
  states = StateListPtr(new std::deque<BoardState>(1));
  pos.set(fen, states->back());

  for (const std::string &move : moves) {
    Move m = UCI::toMove(pos, move);
    if (!m)
      break;
    states->emplace_back();
    pos.makeMove(m, states->back());
  }
}

void Engine::setOption(const std::string &name, const std::string &value) {
  if (compareStr(name, "Hash")) {
    size_t mb = std::stoi(value);
    if (mb != tt.size())
      tt.resize(mb, threads);
  } else if (compareStr(name, "Threads")) {
    size_t n = std::stoi(value);
    if (n != threads.size())
      threads.set(n, searchState);
  }
}

void Engine::perft(Limits &limits) { perftTest(pos, limits.depth); }

void Engine::bench() { perftBench(BENCH_FILE.data()); }

void Engine::go(Limits &limits) {
  threads.stop = threads.abortedSearch = false;

  if constexpr (USE_BOOK) {
    Move move = book.probe(pos);
    if (move) {
      std::cout << "bestmove " << move2Str(move) << std::endl;
      return;
    }
  }

  threads.startThinking(pos, states, limits);
}

void Engine::stop() { threads.stop = threads.abortedSearch = true; }

void Engine::clear() { waitForSearchFinish(); }

}; // namespace Maestro
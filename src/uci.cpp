#include <iostream>
#include <sstream>

#include "bitboard.hpp"
#include "hash.hpp"
#include "nnue.hpp"
#include "perft.hpp"
#include "position.hpp"
#include "search.hpp"
#include "uci.hpp"

namespace Maestro {

void TimeManager::init(Limits &limits, Colour us, int ply,
                       const Config &config) {

  startTime = limits.startTime;
  // If we have no time, we don't need to continue
  if (limits.time[us] == 0)
    return;

  const TimePt time = limits.time[us];
  const TimePt inc = limits.inc[us];

  int mtg = limits.movesToGo ? std::min(limits.movesToGo, 50) : 50;

  if (limits.movesToGo > 0) {
    // X / Y + Z time management
    optimumTime = 1.80 * (time - MOVE_OVERHEAD) / mtg + inc;
    maximumTime = 10.00 * (time - MOVE_OVERHEAD) / mtg + inc;
  } else {
    // X + Y time management
    optimumTime = 2.50 * (time - MOVE_OVERHEAD + 25 * inc) / 50;
    maximumTime = 10.00 * (time - MOVE_OVERHEAD + 25 * inc) / 50;
  }

  // Cap time allocation using move overhead
  optimumTime = std::min(optimumTime, time - MOVE_OVERHEAD);
  maximumTime = std::min(maximumTime, time - MOVE_OVERHEAD);

  std::cout << "Optimum: " << optimumTime << std::endl;
  std::cout << "Maximum: " << maximumTime << std::endl;
}

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
  // Initialize bitboards
  initBitboards();
  // Initialize zobrist keys
  initZobrist();
  // Initialize evaluation
  Eval::initEval();
  // Initialize polyglot book
  initPolyBook(book, BOOK_FILE.data());
  // Initialize nnue file
  nnue_init(NNUE_FILE.data());
  // Initialise thread pool
  resizeThreads(config.threads);
  // Initialise LMR values
  initLMR();
  // Initialise transposition table
  tt.resize(config.hashSize, threads);
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
  threads.set(n, searchState);
  config.threads = n;
}

void Engine::setTTSize(size_t mb) {
  tt.resize(mb, threads);
  config.hashSize = mb;
}

std::string Engine::fen() const { return pos.fen(); }

// Print engine state
void Engine::print() const { pos.print(); }

// Set position
void Engine::setPosition(const std::string fen,
                         const std::vector<std::string> &moves) {
  states = StateListPtr(new std::deque<BoardState>(1));
  pos.set(fen, states->back(), threads.main());

  for (const std::string &move : moves) {
    Move m = UCI::toMove(pos, move);
    if (!m)
      break;
    states->emplace_back();
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

  if (name == "Hash") {
    is >> value; // Get value
    setTTSize(value);
  } else if (name == "Threads") {
    is >> value; // Get value
    resizeThreads(value);
  } else if (name == "MultiPV") {
    is >> value;
    config.multiPV = value;
  } else if (name == "Clear") {
    is >> token;
    if (token == "Hash")
      tt.clear(threads);
  }
}

void Engine::perft(Limits &limits) { perftTest(pos, limits.depth); }

void Engine::bench() { perftBench(threads, BENCH_FILE.data()); }

void Engine::go(Limits &limits) {

  if (DEFAULT_USE_BOOK and !limits.infinite) {
    Move bookMove = getPolyBookMove(book, pos);

    if (bookMove != Move::none()) {
      std::cout << "bestmove " << move2Str(bookMove) << std::endl;
      return;
    }
  }
  threads.startThinking(pos, states, limits);
}

void Engine::stop() { threads.stop = true; }

void Engine::clear() {
  waitForSearchFinish();

  tt.clear(threads);
  threads.clear();
}

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
      std::cout << "version " << VERSION << std::endl;
      // UCI options
      std::cout << "option name Hash type spin default 64 min 1 max 1024"
                << std::endl;
      std::cout << "option name Threads type spin default 1 min 1 max 10"
                << std::endl;
      std::cout << "option name MultiPV type spin default 1 min 1 max 10"
                << std::endl;
      std::cout << "option name Clear Hash type button" << std::endl;

      std::cout << "uciok" << std::endl;
    } else if (token == "isready") {
      std::cout << "readyok" << std::endl;
    } else if (token == "quit" || token == "stop") {
      engine.stop();
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
    if (token == "searchmoves") {
      while (is >> token)
        limits.searchMoves.push_back(to_lower(token));
    } else if (token == "wtime") {
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
    } else if (token == "mate") {
      is >> limits.mate;
    } else if (token == "infinite") {
      limits.infinite = true;
    } else if (token == "perft") {
      limits.perft = true;
      is >> limits.depth;
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

void UCI::uciReport(const PrintInfo &info) {
  int score = (info.score >= VAL_MATE_BOUND) ? (VAL_MATE - info.score + 1) / 2
              : (info.score <= -VAL_MATE_BOUND) ? (-VAL_MATE - info.score) / 2
                                                : info.score;

  std::string type = abs(info.score) >= VAL_MATE_BOUND ? "mate" : "cp";

  std::cout << "info"
            << " depth " << info.depth << " seldepth " << info.selDepth
            << " multipv " << info.multiPVIdx << " score " << type << " "
            << score << " time " << info.timeMs << " nodes " << info.nodes
            << " nps " << info.nps << " hashfull " << info.hashFull << " pv "
            << info.pv << std::endl;
}

} // namespace Maestro

#include <iostream>
#include <sstream>

#include "bitboard.hpp"
#include "defs.hpp"
#include "hashtable.hpp"
#include "movegen.hpp"
#include "nnue.hpp"
#include "perft.hpp"
#include "polyglot.hpp"
#include "position.hpp"
#include "uci.hpp"
#include "utils.hpp"

constexpr std::string_view NAME = "Maestro";
constexpr std::string_view AUTHOR = "Evan Fung";

// Constructor
UCI::UCI() {
  initBitboards();
  initZobrist();
  TTInit(16);
  nnue_init("nn-eba324f53044.nnue");
  initPolyBook("baron30.bin");
}

// Main loop of the chess engine
void UCI::loop() {
  std::string input, token;

  Position pos;
  StateList states{new std::deque<BoardState>(1)};

  // Start from start position
  pos.set(startPos.data(), states->back());

  do {
    std::getline(std::cin, input);

    std::stringstream is(input);

    token.clear();
    is >> std::skipws >> token;

    if (token == "uci") {
      std::cout << "id name " << NAME << std::endl;
      std::cout << "id author " << AUTHOR << std::endl;
      std::cout << "uciok" << std::endl;
    } else if (token == "isready") {
      std::cout << "readyok" << std::endl;
    } else if (token == "quit" || token == "stop") {
      worker.stop = true;
    } else if (token == "ucinewgame") {
      searchClear();
    } else if (token == "go") {
      parseGo(is, pos, states);
    } else if (token == "position") {
      parsePosition(is, pos, states);
    } else if (token == "b") {
      pos.print();
    }
  } while (token != "quit");

  worker.quit = true;
  worker.waitForSearchFinished();
}

// Parse the UCI go command
void UCI::parseGo(std::stringstream &command, Position &pos,
                  StateList &states) {
  std::string token;

  Limits limits{};

  Time wtime = 0, btime = 0, winc = 0, binc = 0, movetime = 0;
  int movestogo = 0;
  int depth = 0;
  bool perft = false;

  command >> std::skipws;

  while (command >> token) {
    if (token == "infinite") {
      limits.infinite = true;
    } else if (token == "wtime") {
      command >> wtime;
    } else if (token == "btime") {
      command >> btime;
    } else if (token == "winc") {
      command >> winc;
    } else if (token == "binc") {
      command >> binc;
    } else if (token == "movestogo") {
      command >> movestogo;
    } else if (token == "depth") {
      command >> depth;
      limits.depthSet = true;
    } else if (token == "movetime") {
      command >> movetime;
      limits.timeSet = true;
    } else if (token == "perft") {
      perft = true;
      command >> depth;
      break;
    }
  }

  if (perft) {
    perftTest(pos, depth);
    return;
  }

  if (depth == 0 || limits.infinite) {
    depth = MAX_DEPTH;
  }

  limits.selfControl = !limits.infinite && !limits.depthSet && !limits.timeSet;

  limits.start = getTimeMs();
  limits.time = (pos.getSideToMove() == WHITE) ? wtime : btime;
  limits.inc = (pos.getSideToMove() == WHITE) ? winc : binc;
  limits.movesToGo = movestogo;

  if (limits.timeSet) {
    limits.timeLimit = movetime;
  }

  if (!limits.infinite) {
    Move bookMove = getPolyBookMove(pos);

    if (bookMove != Move::none()) {
      std::cout << "bestmove " << move2Str(bookMove) << std::endl;
      return;
    }
  }

  TimeControl time{};
  parseTime(limits, time);

  std::cout << "Time allocated (Ideal): " << time.idealStopTime << std::endl;
  std::cout << "Time allocated (Max): " << time.maxStopTime << std::endl;
  std::cout << "Depth: " << depth << std::endl;
  // Start searching
  worker.start(pos, states, time, depth);
}

// Parse the UCI position command
void UCI::parsePosition(std::stringstream &command, Position &pos,
                        StateList &states) {
  std::string token, fen;

  command >> token;

  if (token == "startpos") {
    fen = startPos.data();
    command >> token;
  } else if (token == "fen") {
    while (command >> token && token != "moves") {
      fen += token + " ";
    }
  } else {
    return;
  }

  states = StateList{new std::deque<BoardState>(1)};
  pos.set(fen.data(), states->back());

  Move m;
  while (command >> token && (m = parseMove(token, pos)) != Move::none()) {
    states->emplace_back();
    pos.makeMove(m, states->back());
  }
}

// Parse moves
Move UCI::parseMove(std::string_view token, const Position &pos) {
  // Generate all moves
  Move moves[256];
  Move *last = generateMoves<ALL>(moves, pos);

  for (Move *begin = moves; begin < last; ++begin) {
    if (token == move2Str(*begin)) {
      return *begin;
    }
  }

  return Move::none();
}

// Parse go time controls
void UCI::parseTime(const Limits &limits, TimeControl &tc) {
  tc.startTime = limits.start;

  // Time management
  if (limits.selfControl) {
    if (limits.movesToGo > 0) {
      tc.idealStopTime =
          1.8 * (limits.time - MOVE_OVERHEAD) / (limits.movesToGo + 5) +
          limits.inc;
      tc.maxStopTime =
          10 * (limits.time - MOVE_OVERHEAD) / (limits.movesToGo + 10) +
          limits.inc;
    } else {
      tc.idealStopTime =
          2.5 * ((limits.time - MOVE_OVERHEAD) + 25 * limits.inc) / 50;
      tc.maxStopTime =
          10 * ((limits.time - MOVE_OVERHEAD) + 25 * limits.inc) / 50;
    }

    // Make sure the time doesn't get too low
    tc.idealStopTime = std::min(tc.idealStopTime, limits.time - MOVE_OVERHEAD);
    tc.maxStopTime = std::min(tc.maxStopTime, limits.time - MOVE_OVERHEAD);
  }

  if (limits.timeSet) {
    tc.idealStopTime = limits.timeLimit;
    tc.maxStopTime = limits.timeLimit;
  }
}

void UCI::searchClear() {
  TTClear();
  worker.stop = true;
  worker.waitForSearchFinished();
}

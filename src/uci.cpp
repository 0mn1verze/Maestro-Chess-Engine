#include <iostream>
#include <sstream>
#include <string>

#include "defs.hpp"
#include "engine.hpp"
#include "movegen.hpp"
#include "uci.hpp"
#include "utils.hpp"

namespace Maestro {

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
      std::cout << "uciok" << std::endl;

      // Communicate supported options
      std::cout << "option Hash type spin default 64 min 1 max 256\n";
      std::cout << "option Threads type spin default 1 min 1 max 12\n";
    } else if (token == "isready") {
      std::cout << "readyok" << std::endl;
    } else if (token == "quit" || token == "stop") {
      engine.stop();
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
    } else if (token == "setoption") {
      setOption(is);
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
      is >> limits.depth;
    }
  }

  return limits;
}

// Parse the UCI go command
void UCI::go(std::istringstream &is) {

  Limits limits = parseLimits(is);

  std::cout << "info string Searching depth " << limits.depth << std::endl;

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

void UCI::setOption(std::istringstream &is) {
  std::string token, name, value;

  is >> token; // Consume name token

  // Parse option name
  while (is >> token && token != "value")
    name += (name.empty() ? "" : " ") + token;

  // Parse option value
  while (is >> token)
    value += (value.empty() ? "" : " ") + token;

  engine.setOption(name, value);
}

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

void UCI::uciReportCurrentMove(Depth depth, Move move, int currmove) {
  std::cout << "info depth " << depth << " currmove " << move2Str(move)
            << " currmovenumber " << currmove << std::endl;
}

void UCI::uciReportNodes(ThreadPool &threads, int hashFull, TimePt elapsed) {
  U64 nodes = threads.nodesSearched();
  std::cout << "info nodes " << nodes << " nps " << nodes * 1000 / elapsed
            << " hashfull " << hashFull << std::endl;
}

void UCI::uciReport(const PrintInfo &info) {
  int score = (info.score >= VAL_MATE_BOUND) ? (VAL_MATE - info.score + 1) / 2
              : (info.score <= -VAL_MATE_BOUND) ? (-VAL_MATE - info.score) / 2
                                                : info.score;

  std::string type = abs(info.score) >= VAL_MATE_BOUND ? "mate" : "cp";

  std::cout << "info"
            << " depth " << info.depth << " seldepth " << info.selDepth
            << " score " << type << " " << score << " time " << info.timeMs
            << " nodes " << info.nodes << " nps " << info.nps << " hashfull "
            << info.hashFull << " pv " << info.pv << std::endl;
}

} // namespace Maestro
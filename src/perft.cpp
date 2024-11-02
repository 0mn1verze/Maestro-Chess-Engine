#include "perft.hpp"

#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "defs.hpp"
#include "movegen.hpp"
#include "position.hpp"
#include "thread.hpp"
#include "utils.hpp"

namespace Maestro {

U32 perftDriver(Position &pos, int depth) {
  // Generate all moves
  MoveList<ALL> moves(pos);

  // Return move count (bulk counting)
  if (depth == 1)
    return moves.size();

  U32 nodes = 0;
  BoardState st{};

  // Loop through all moves
  for (GenMove move : moves) {
    pos.makeMove(move, st);
    // Recurse if depth > 1
    nodes += perftDriver(pos, depth - 1);

    pos.unmakeMove();
  }

  return nodes;
}

void perftTest(Position &pos, int depth) {

  // Print depth
  std::cout << "\n\n	Perft Test: Depth " << depth << std::endl;
  std::cout << "\n\n";
  U64 start = getTimeMs();
  // Init node variable
  U32 nodes = 0;

  // Generate all moves
  MoveList<ALL> moves(pos);
  // Init node count
  U32 count = 1;
  BoardState st{};
  // Loop through all moves
  for (GenMove move : moves) {
    // Make move
    pos.makeMove(move, st);
    // Recurse if depth > 1
    count = 1;
    if (depth > 1)
      count = perftDriver(pos, depth - 1);
    // Unmake move
    pos.unmakeMove();
    // Print move and node count (For debugging)
    std::cout << "	Move: " << move2Str(move) << " Nodes: " << count
              << std::endl;
    // Add to total node count
    nodes += count;
  }
  // End clock
  U64 duration = getTimeMs() - start;
  if (duration == 0)
    duration = 1;
  // Print results
  std::cout << "\n\n==========================================\n" << std::endl;
  std::cout << "	Total Nodes:	" << nodes << std::endl;
  std::cout << "	Duration:	" << duration << " ms" << std::endl;
  std::cout << "	Performance:	" << nodes / 1000 / duration << " MNPS"
            << std::endl;
  std::cout << "\n==========================================\n\n";
}

std::vector<PerftPosition> readBenchFile(std::string filePath) {
  std::ifstream bench;
  // Open bench file
  bench.open(filePath);
  // Create positions vector
  std::vector<PerftPosition> positions;
  // Check if file is open
  if (!bench.is_open()) {
    std::cout << "Error: Could not open bench file" << std::endl;
    return positions;
  }
  // Init positions vector
  positions.clear();
  // Init variables
  std::string line, cell;
  // Init perft position variable
  PerftPosition pos;
  // Read positions
  while (std::getline(bench, line)) {
    // Init stringstream
    std::stringstream lineStream(line);
    // Read FEN
    std::getline(lineStream, cell, ',');
    pos.fen = cell;
    // Read depth
    std::getline(lineStream, cell, ',');
    pos.depth = std::stoi(cell);
    // Read nodes
    std::getline(lineStream, cell, ',');
    pos.nodes = std::stoull(cell);
    // Add position to vector
    positions.push_back(pos);
  }
  std::cout << "	Bench file read successfully" << std::endl;
  // Close file
  bench.close();
  // Return positions
  return positions;
}

void perftBench(ThreadPool &threads, std::string filePath) {
  // Read bench file
  std::vector<PerftPosition> positions = readBenchFile(filePath);
  // Init position and nodes variable
  Position pos;
  U32 nodes;
  BoardState st{};
  // Loop through all positions
  for (PerftPosition p : positions) {
    // Set position
    pos.set(p.fen, st, threads.main());
    // Get time
    U64 start = getTimeMs();
    // Run perft test
    nodes = perftDriver(pos, p.depth);
    // End clock
    U64 duration = getTimeMs() - start;
    if (duration == 0)
      duration = 1;
    if (p.nodes == nodes) {
      std::cout << "	Perft Test: " << p.fen << " Depth: " << p.depth
                << " Nodes: " << p.nodes << " Passed in " << duration
                << " ms with " << p.nodes / 1000 / duration << " Mnps"
                << std::endl;
    } else {
      std::cout << "	Perft Test: " << p.fen << " Depth: " << p.depth
                << " Nodes: " << p.nodes << "(" << nodes << ")"
                << " Failed in " << duration << " ms with "
                << nodes / 1000 / duration << " Mnps" << std::endl;
    }
  }
}

} // namespace Maestro

#include "perft.hpp"

#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "defs.hpp"
#include "movegen.hpp"
#include "position.hpp"
#include "utils.hpp"

Count perftDriver(Position &pos, int depth) {
  // Generate Moves
  Move moves[256];
  Move *last = generateMoves<ALL>(moves, pos);

  // Return move count (bulk counting)
  if (depth == 1)
    return last - moves;

  Count nodes = 0;

  BoardState st{};
  // Loop through all moves
  for (Move *begin = moves; begin < last; ++begin) {
    pos.makeMove(*begin, st);
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
  Time start = getTimeMs();
  // Init node variable
  Count nodes = 0;

  // Generate all moves
  Move moves[256];
  refreshMasks(pos);
  Move *last = generateMoves<ALL>(moves, pos);
  // Init node count
  Count count = 1;
  BoardState st{};
  // Loop through all moves
  for (Move *begin = moves; begin < last; ++begin) {
    // Make move
    pos.makeMove(*begin, st);
    // Recurse if depth > 1
    count = 1;
    if (depth > 1)
      count = perftDriver(pos, depth - 1);
    // Unmake move
    pos.unmakeMove();
    // Print move and node count (For debugging)
    std::cout << "	Move: " << move2Str(*begin) << " Nodes: " << count
              << std::endl;
    // Add to total node count
    nodes += count;
  }
  // End clock
  Time duration = getTimeMs() - start;
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

std::vector<PerftPosition> readBenchFile() {
  std::ifstream bench;
  // Open bench file
  bench.open("../src/bench.csv");
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

void perftBench() {
  // Read bench file
  std::vector<PerftPosition> positions = readBenchFile();
  // Init position and nodes variable
  Position pos;
  Count nodes;
  BoardState st{};
  // Loop through all positions
  for (PerftPosition p : positions) {
    // Set position
    pos.set(p.fen, st);
    // Get time
    Time start = getTimeMs();
    // Run perft test
    nodes = perftDriver(pos, p.depth);
    // End clock
    Time duration = getTimeMs() - start;
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

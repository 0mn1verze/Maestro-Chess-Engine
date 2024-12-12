#ifndef HISTORY_HPP
#pragma once
#define HISTORY_HPP

#include <cstring>

#include "defs.hpp"
#include "move.hpp"
#include "position.hpp"
#include "utils.hpp"

namespace Maestro {

/******************************************\
|==========================================|
|         Template for Heuristics          |
|==========================================|
\******************************************/

namespace Heuristic {

template <typename T, int D> class Entry {
public:
  T entry;

  operator const T &() const { return entry; }

  void operator<<(int bonus) {
    bonus = std::clamp(bonus, -D, D);
    entry += bonus - entry * abs(bonus) / D;
  }
};

} // namespace Heuristic

enum {
  NOT_USED = 0,
  HISTORY = 16384,
};

/******************************************\
|==========================================|
|            Table Definitions             |
|==========================================|
\******************************************/

using Killer = Heuristic::Entry<Move, NOT_USED>;
using History = Heuristic::Entry<Value, HISTORY>;

// Killer moves table
// https://www.chessprogramming.org/Killer_Heuristic
// A move ordering technique for quiet moves that stores moves that caused a
// beta-cutoff in a sibling node and orders then high on the list. Example:
// Maybe there is a back rank mate that can only be prevented by one move, so
// the move causes a beta cutoff in most cases and is ordered high on the list
// when the sibling nodes are searched, therefore the engine knows to evaluate
// the possible mate first, as it is likely a good move.

// Killer moves table [ply][move number]
struct KillerTable {

  void clear() { memset(table, 0, sizeof(table)); }
  Move &probe(int ply, int moveNumber) { return table[ply][moveNumber].entry; }
  void update(int ply, Move move);

private:
  Killer table[MAX_PLY + 1][2];
};

// History table
// https://www.chessprogramming.org/History_Heuristic
// A move ordering technique for quiet moves that stores how good a move was
// regardless of the position it was in. If a move caused a beta cutoff in the
// search, it is likely a good move in other positions so it receives a bonus
// and is moved higher on the move list, so it can be searched first. Example:
// Maybe a bishop move traps the queen in a lot of cases due to low mobility for
// the opponent, if so the move will make its way up to move list hiearchy as
// the search realises the move is a better than the others.

// History table [piece][to]

struct HistoryTable {

  void clear() { memset(table, 0, sizeof(table)); }

  History &probe(const Position &pos, Move move);

  void update(const Position &pos, Move move, Value bonus) {
    probe(pos, move) << bonus;
  }

private:
  History table[2][2][PIECE_N][SQ_N];
};

// Capture history table
// https://www.chessprogramming.org/Capture_Heuristic
// Same as history table but for captures instead

// Capture history table [piece][evade threat][enters threat][to][captured]
struct CaptureHistoryTable {
  void clear() { memset(table, 0, sizeof(table)); }

  History &probe(const Position &pos, Move move);

  void update(const Position &pos, Move move, Value bonus) {
    probe(pos, move) << bonus;
  }

private:
  History table[PIECE_N][2][2][SQ_N][PIECE_N];
};

// Continuation history table
// https://www.chessprogramming.org/Continuation_Heuristic
// A move ordering technique for quiet moves that stores how good a move is with
// respect to a particular root move A different table is assign to each node
// according to the piece moved and the destination square, then it stores the
// relative value of the move with respect to the sequence of moves before it.

// The current implementation uses a 4 ply history tree, so there is a 4 ply
// sequence of moves that the continution history stores.

// Continuation history [piece][to]
struct Continuation {
  void clear() { memset(table, 0, sizeof(table)); }

  History &probe(const Position &pos, Move move);

  void update(const Position &pos, Move move, Value bonus) {
    probe(pos, move) << bonus;
  }

private:
  History table[PIECE_N][SQ_N];
};

// Continuation history table [inCheck][isCapture][piece][to]
struct ContinuationHistoryTable {

  void clear();

  Continuation table[2][2][PIECE_N][SQ_N];
};

} // namespace Maestro

#endif
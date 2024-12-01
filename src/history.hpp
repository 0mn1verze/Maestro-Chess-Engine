#ifndef HISTORY_HPP
#pragma once
#define HISTORY_HPP

#include "defs.hpp"
#include "move.hpp"
#include "position.hpp"
#include "utils.hpp"

namespace Maestro {

/******************************************\
|==========================================|
|       Template for Heuristics tables     |
|==========================================|
\******************************************/

template <typename T, int D, int... N> class Heuristic {
public:
  void clear(T value) { _table.fill(value); }

  void update(T &entry, T bonus) {
    bonus = std::clamp(bonus, -D, D);
    entry += bonus - entry * abs(bonus) / D;
  }

protected:
  Array<T, N...> _table;
};

enum {
  NOT_USED = 0,
  HISTORY = 16384,
};

/******************************************\
|==========================================|
|            Table Definitions             |
|==========================================|
\******************************************/

// Killer moves table
// https://www.chessprogramming.org/Killer_Heuristic
// A move ordering technique for quiet moves that stores moves that caused a
// beta-cutoff in a sibling node and orders then high on the list. Example:
// Maybe there is a back rank mate that can only be prevented by one move, so
// the move causes a beta cutoff in most cases and is ordered high on the list
// when the sibling nodes are searched, therefore the engine knows to evaluate
// the possible mate first, as it is likely a good move.

struct KillerTable : public Heuristic<Move, NOT_USED, MAX_PLY + 1, 2> {

  // Update killer moves
  void update(Move move, int ply);

  // Clear killer moves table
  void clear() { Heuristic::clear(Move::none()); }

  // Get killer move entry (const)
  Move &probe(int ply, int moveN) { return _table[ply][moveN]; }
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

struct HistoryTable
    : public Heuristic<Value, HISTORY, COLOUR_N, 2, 2, SQ_N, SQ_N> {

  // Update history table entry
  void update(const Position &pos, Move move, Value value);

  // Clear history table
  void clear() { Heuristic::clear(0); }

  // Probe history table
  Value &probe(const Position &pos, Move move);
};

// Capture history table
// https://www.chessprogramming.org/Capture_Heuristic
// Same as history table but for captures instead

struct CaptureHistoryTable : public Heuristic<Value, HISTORY, PIECE_TYPE_N, 2,
                                              2, SQ_N, PIECE_TYPE_N - 1> {
public:
  // Update capture history table entry
  void update(const Position &pos, Move move, Value value);

  // Clear capture history table
  void clear() { Heuristic::clear(0); }

  // Probe capture history table
  Value &probe(const Position &pos, Move move);
};

} // namespace Maestro

#endif
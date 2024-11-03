#ifndef MOVEPICKER_HPP
#define MOVEPICKER_HPP

#include "defs.hpp"
#include "move.hpp"
#include "movegen.hpp"
#include "position.hpp"
#include "utils.hpp"

namespace Maestro {

/******************************************\
|==========================================|
|          Heuristics Definition           |
|==========================================|
\******************************************/

template <typename T, int D> struct StatsEntry {
  T entry;

public:
  void operator=(const T &v) { entry = v; }
  T *operator&() { return &entry; }
  T *operator->() { return &entry; }
  operator const T &() const { return entry; }

  void operator<<(int bonus) {
    bonus = std::clamp(bonus, -D, D);
    entry += bonus - entry * abs(bonus) / D;
  }
};

template <typename T, int D, int N, int... M>
struct Stats : public std::array<Stats<T, D, M...>, N> {
  using stats = Stats<T, D, N, M...>;

  void fill(const T &value) {

    using Entry = StatsEntry<T, D>;
    Entry *begin = reinterpret_cast<Entry *>(this);
    std::fill(begin, begin + sizeof(*this) / sizeof(Entry), value);
  }
};

template <typename T, int D, int N>
struct Stats<T, D, N> : public std::array<StatsEntry<T, D>, N> {};

/******************************************\
|==========================================|
|           Heuristic Definitions          |
|==========================================|
\******************************************/

enum { NOT_USED, HISTORY = 16384 };

// Killer table [Ply][Move]
using KillerTable = Stats<Move, NOT_USED, MAX_DEPTH + 1, 2>;

// Counter move table [Colour][PieceType][To]
using CounterMoveTable = Stats<Move, NOT_USED, COLOUR_N, PIECE_TYPE_N, SQ_N>;

// History table [Colour][IsThreatened][IsAttacking][From][To]
using HistoryTable = Stats<Value, HISTORY, COLOUR_N, 2, 2, SQ_N, SQ_N>;

// Capture history table
// [MovedPiece][IsThreatened][IsAttacking][To][AttackedPiece]
using CaptureHistoryTable =
    Stats<Value, HISTORY, PIECE_TYPE_N, 2, 2, SQ_N, PIECE_TYPE_N - 1>;

// Continuation History [Piece][To]
using ContinuationHistory = Stats<Value, HISTORY, PIECE_N, SQ_N>;

// Continuation table [IsCapture][Piece][To][Continuation Number]
using ContinuationTable =
    Stats<ContinuationHistory, NOT_USED, 2, PIECE_N, SQ_N, CONT_N>;

/******************************************\
|==========================================|
|           MovePicker Definition          |
|==========================================|
\******************************************/

class MovePicker {

public:
  MovePicker(const Position &pos, GenMove ttm, Depth depth,
             const KillerTable *kt, const CounterMoveTable *cmt,
             const HistoryTable *ht, const CaptureHistoryTable *cht,
             const ContinuationHistory **ch, int ply);
  MovePicker(const Position &pos, GenMove ttm, int threshold,
             const CaptureHistoryTable *cht);

  Move selectNext(bool skipQuiets);

private:
  GenMove *begin() { return cur; }
  GenMove *end() { return endMoves; }

  template <GenType Type> void score();

  GenMove bestMove();

  bool isSpecial(GenMove *move) {
    return move->move == ttMove || move->move == counterMove ||
           move->move == killer1 || move->move == killer2;
  }

  const KillerTable *kt;
  const CounterMoveTable *cmt;
  const HistoryTable *ht;
  const CaptureHistoryTable *cht;
  const ContinuationHistory **ch;

  const Position &pos;

  GenMove ttMove, killer1, killer2, counterMove;
  GenMove *cur, *endCaptures, *startQuiet, *endQuiet, *endMoves;
  int stage;
  int threshold;
  Depth depth;
  int ply;
  bool skipQuiets;
  GenMove moves[MAX_MOVES];
};

} // namespace Maestro

#endif // MOVEPICKER_HPP

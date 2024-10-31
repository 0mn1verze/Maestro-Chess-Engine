#ifndef MOVEPICKER_HPP
#define MOVEPICKER_HPP

#include "defs.hpp"
#include "move.hpp"
#include "position.hpp"
#include "utils.hpp"

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

using KillerTable = Stats<Move, NOT_USED, MAX_DEPTH + 1, 2>;
using CounterMoveTable = Stats<Move, NOT_USED, COLOUR_N, PIECE_N, SQ_N>;
using HistoryTable = Stats<U16, HISTORY, COLOUR_N, 2, 2, SQ_N, SQ_N>;
using CaptureHistoryTable =
    Stats<U16, HISTORY, PIECE_N, 2, 2, SQ_N, PIECE_N - 1>;
using ContinuationTable =
    Stats<U16, HISTORY, 2, PIECE_N, SQ_N, CONT_N, PIECE_N, SQ_N>;

#endif // MOVEPICKER_HPP

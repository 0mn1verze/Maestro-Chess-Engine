#ifndef MOVEPICKER_HPP
#define MOVEPICKER_HPP

#include "defs.hpp"
#include "movegen.hpp"
#include "position.hpp"

// Move generation stage
enum GenStage { PV, INIT_CAPTURES, G_CAPTURES, INIT_QUIETS, G_QUIETS };

// Increment stage
inline GenStage &operator++(GenStage &gs) { return gs = GenStage(int(gs) + 1); }

// Partial insertion sort
inline void insertion_sort(Move *begin, Move *end, int limit) {
  Move *p;
  for (p = begin + 1; p < end; p++) {
    Move tmp = *p, *j;
    for (j = p; j > begin && *(j - 1) < tmp; j--) {
      *j = *(j - 1);
    }
    *j = tmp;
  }
}

struct SearchStats;

// MovePicker class
class MovePicker {
public:
  // Main search constructor
  MovePicker(Position &pos, SearchStats &ss, const int ply,
             const Depth depth = 0, const Move pvMove = Move::none());

  Move pickNextMove(bool quiscence);

  GenStage genStage = PV;

private:
  template <GenType gt> void scoreMoves();

  Move *begin() { return cur; }
  Move *end() { return back; }

  size_t size() { return back - moves; }

  // Board
  Position &pos;

  SearchStats &ss;

  // Search depth
  const int ply;
  const Depth depth;

  // move list and PV move
  Move moves[MAX_MOVES], *cur, *back, ttMove = Move::none();
};

#endif // MOVEPICKER_HPP
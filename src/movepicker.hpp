#ifndef MOVEPICKER_HPP
#define MOVEPICKER_HPP

#include "defs.hpp"
#include "movegen.hpp"
#include "position.hpp"

// Move generation stage
enum GenStage {
  TT,
  INIT_CAPTURE,
  GOOD_CAPTURE,
  KILLER1,
  KILLER2,
  INIT_QUIET,
  GOOD_QUIET,
  BAD_CAPTURE,
  BAD_QUIET,

  // Quiescence Search
  Q_TT,
  Q_INIT_CAPTURE,
  Q_CAPTURE,

  // Probe cut search
  PROBCUT_TT,
  PROBCUT_INIT,
  PROBCUT
};

// Increment stage
inline GenStage &operator++(GenStage &gs) { return gs = GenStage(int(gs) + 1); }

struct SearchStats;

// MovePicker class
class MovePicker {
  enum PickType { Next, Best };

public:
  // Main / Quiescence search constructor
  MovePicker(Position &pos, SearchStats &ss, const int ply, const Depth depth,
             const Move pvMove = Move::none(), const bool onlyCaptures = false,
             const int threshold = 0);

  // Prob cut search constructor
  MovePicker(Position &pos, SearchStats &ss, const int threshold = 0,
             const Move pvMove = Move::none());

  Move pickNextMove(bool quiscence);

  GenStage genStage = TT;

private:
  template <GenType gt> void scoreMoves();
  template <MovePicker::PickType T, typename Pred> Move select(Pred filter);

  Move *begin() { return cur; }
  Move *end() { return endMoves; }

  size_t size() { return endMoves - moves; }

  // Board
  Position &pos;

  SearchStats &ss;

  // Search depth
  const int ply;
  const Depth depth;
  const bool onlyCaptures;
  const int threshold;

  // move list and PV move
  Move moves[MAX_MOVES], *cur, *endMoves, *endBadCaptures, *endBadQuiets,
      *beginBadQuiets, ttMove = Move::none(), killer1, killer2;
};

#endif // MOVEPICKER_HPP
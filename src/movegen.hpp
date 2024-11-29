#ifndef MOVEGEN_HPP
#pragma once
#define MOVEGEN_HPP

#include "defs.hpp"
#include "position.hpp"

namespace Maestro {

// Move generation types
enum GenType { ALL, CAPTURES, QUIETS };

// Refresh masks
void refreshMasks(const Position &pos);

// Refresh en passant pin
void refreshEPPin(const Position &pos);

// Generate moves
template <GenType gt> Move *generateMoves(Move *moves, const Position &pos);

template <GenType T> struct MoveList {

  explicit MoveList(const Position &pos) : last(generateMoves<T>(moves, pos)) {}
  const Move *begin() const { return moves; }
  const Move *end() const { return last; }
  size_t size() const { return last - moves; }
  Move &operator[](size_t idx) { return moves[idx]; }
  bool empty() const { return moves == last; }
  bool contains(const Move &m) const {
    return std::find(begin(), end(), m) != end();
  }

private:
  Move moves[MAX_MOVES], *last;
};

} // namespace Maestro

#endif // MOVEGEN_HPP
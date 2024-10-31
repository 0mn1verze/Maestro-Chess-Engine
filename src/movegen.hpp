#ifndef MOVEGEN_HPP
#define MOVEGEN_HPP

#include "defs.hpp"
#include "position.hpp"

// Move generation types
enum GenType { ALL, CAPTURES, QUIETS };

struct GenMove {
  // Assignment operator
  void operator=(const Move &m) { move = m; }
  // Move
  Move move;
  // Move score for ordering
  int score;
  // Operators
  bool operator<(const GenMove &m) const { return score < m.score; }
  bool operator>(const GenMove &m) const { return score > m.score; }
  bool operator<=(const GenMove &m) const { return score <= m.score; }
  bool operator>=(const GenMove &m) const { return score >= m.score; }
  bool operator==(const GenMove &m) const { return move == m.move; }
  bool operator!=(const GenMove &m) const { return move != m.move; }
  // Convert to move
  operator Move() const { return move; }
  // Null and none move
  static GenMove none() { return GenMove{Move::none(), 0}; }
  static GenMove null() { return GenMove{Move::null(), 0}; }
};

// Refresh masks
void refreshMasks(const Position &pos);

// Refresh en passant pin
void refreshEPPin(const Position &pos);

// Generate moves
template <GenType gt>
GenMove *generateMoves(GenMove *moves, const Position &pos);

template <GenType T> struct MoveList {

  explicit MoveList(const Position &pos) : last(generateMoves<T>(moves, pos)) {}
  const GenMove *begin() const { return moves; }
  const GenMove *end() const { return last; }
  size_t size() const { return last - moves; }
  GenMove &operator[](size_t idx) { return moves[idx]; }
  bool empty() const { return moves == last; }
  bool contains(const GenMove &m) const {
    return std::find(begin(), end(), m) != end();
  }

private:
  GenMove moves[MAX_MOVES], *last;
};

#endif // MOVEGEN_HPP
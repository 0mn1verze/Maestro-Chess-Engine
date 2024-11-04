#ifndef MOVE_HPP
#define MOVE_HPP

#include <cstdint>
#include <cstdlib>

#include "defs.hpp"

namespace Maestro {

/******************************************\
|==========================================|
|            Move representation           |
| Bits (0-5): from square                  |
| Bits (6-11): to square                   |
| Bits (12-13): flags                      |
| Bits (14-15): promoted piece type        |
|==========================================|
\******************************************/

// Move flags
enum MoveFlag {
  NORMAL = 0,
  EN_PASSANT = 1,
  PROMOTION = 2,
  CASTLE = 3,
  ALL_FLAGS = 3
};

// Move representation
class Move {
public:
  // Constructors
  Move() = default;
  explicit Move(U16 m) : data(m){};
  Move &operator=(const Move &m) = default;
  // Encode move
  template <MoveFlag flag = NORMAL>
  static inline Move encode(Square from, Square to,
                            PieceType promoted = KNIGHT) {
    return Move(from | to << 6 | flag << 12 | (promoted - KNIGHT) << 14);
  }
  // Data accessors
  inline Square from() const { return Square(data & static_cast<int>(H8)); }
  inline Square to() const {
    return Square((data >> 6) & static_cast<int>(H8));
  }
  MoveFlag flag() const { return MoveFlag((data >> 12) & CASTLE); }
  inline PieceType promoted() const { return PieceType((data >> 14) + KNIGHT); }
  inline int raw() const { return data; }
  // Move flags
  template <MoveFlag f> inline bool is() const { return flag() == f; }
  // Operators
  bool operator==(const Move &m) const { return data == m.raw(); }
  bool operator!=(const Move &m) const { return data != m.raw(); }

  // Check if move is valid
  operator bool() const {
    return data != none().raw() and data != null().raw();
  }
  // No move
  static Move none() { return Move(0); }
  // Null move
  static Move null() { return Move::encode(B1, B1); }

private:
  U16 data;
};

} // namespace Maestro

#endif // MOVE_HPP
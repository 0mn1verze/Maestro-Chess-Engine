#ifndef MOVEGEN_HPP
#define MOVEGEN_HPP

#include "defs.hpp"
#include "position.hpp"

namespace MoveGen {

// Move generation types
enum GenType { ALL, CAPTURES, QUIETS };

// Refresh masks
void refreshMasks(const Position &pos);

// Refresh en passant pin
void refreshEPPin(const Position &pos);

// Generate moves
template <GenType gt> Move *generateMoves(Move *moves, const Position &pos);

} // namespace MoveGen

#endif // MOVEGEN_HPP
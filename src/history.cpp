#include "history.hpp"
#include "defs.hpp"
#include "move.hpp"
#include "position.hpp"
#include "utils.hpp"

namespace Maestro {

void KillerTable::update(Move move, int ply) {
  if (move != _table[ply][0]) {
    _table[ply][1] = _table[ply][0];
    _table[ply][0] = move;
  }
}

Value &HistoryTable::probe(const Position &pos, Move move) {
  const Square to = move.to();
  const Square from = move.from();

  const Colour us = pos.sideToMove();

  const bool threatFrom = pos.attacked() & from;
  const bool threatTo = pos.attacked() & to;

  return _table[us][threatFrom][threatTo][from][to];
}

void HistoryTable::update(const Position &pos, Move move, Value value) {
  Heuristic::update(probe(pos, move), value);
}

Value &CaptureHistoryTable::probe(const Position &pos, Move move) {
  const Square to = move.to();
  const Square from = move.from();

  const PieceType captured = pos.capturedPiece(move);
  const PieceType piece = pos.movedPieceType(move);

  const bool threatFrom = pos.state()->attacked & from;
  const bool threatTo = pos.state()->attacked & to;

  return _table[piece][threatFrom][threatTo][to][captured];
};

void CaptureHistoryTable::update(const Position &pos, Move move, Value value) {
  Heuristic::update(probe(pos, move), value);
}

} // namespace Maestro
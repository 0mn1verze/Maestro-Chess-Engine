#include "history.hpp"

namespace Maestro {

void KillerTable::update(int ply, Move move) {
  if (move != table[ply][0].entry) {
    table[ply][1].entry = table[ply][0].entry;
    table[ply][0].entry = move;
  }
}

History &HistoryTable::probe(const Position &pos, Move move) {
  const Square to = move.to();
  const Square from = move.from();

  const Colour us = pos.sideToMove();

  const Piece piece = pos.movedPiece(move);

  const bool threatFrom = pos.attacked() & from;
  const bool threatTo = pos.attacked() & to;

  return table[threatFrom][threatTo][piece][to];
}

History &CaptureHistoryTable::probe(const Position &pos, Move move) {
  const Square to = move.to();
  const Square from = move.from();

  const Colour us = pos.sideToMove();

  const Piece piece = pos.movedPiece(move);
  const PieceType captured = pos.capturedPiece(move);

  const bool threatFrom = pos.attacked() & from;
  const bool threatTo = pos.attacked() & to;

  return table[captured][threatFrom][threatTo][to][piece];
}

History &Continuation::probe(const Position &pos, Move move) {
  const Square to = move.to();
  const Piece piece = pos.movedPiece(move);

  return table[piece][to];
}

void ContinuationHistoryTable::clear() {
  for (int i = 0; i < 2; i++)
    for (int j = 0; j < 2; j++)
      for (int k = 0; k < PIECE_N; k++)
        for (int l = 0; l < SQ_N; l++)
          table[i][j][k][l].clear();
}

} // namespace Maestro
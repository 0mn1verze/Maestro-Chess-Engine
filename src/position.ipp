#ifndef POSITION_IMPL_HPP
#define POSITION_IMPL_HPP

#include "bitboard.hpp"
#include "defs.hpp"
#include "move.hpp"
#include "movegen.hpp"
#include "position.hpp"
#include "utils.hpp"

/******************************************\
|==========================================|
|            Template functions            |
|==========================================|
\******************************************/

// Get square of a piece type with the lowest index
template <PieceType pt> Square Position::square(Colour us) const {
  return getLSB(getPiecesBB(us, pt));
}

// Get the bitboard of pieces of certain types
template <typename... PieceTypes>
Bitboard Position::getPiecesBB(PieceType pt, PieceTypes... pts) const {
  return piecesBB[pt] | getPiecesBB(pts...);
}

// Get the bitboard of pieces of certain types for a colour
template <typename... PieceTypes>
Bitboard Position::getPiecesBB(Colour us, PieceTypes... pts) const {
  return occupiedBB[us] & getPiecesBB(pts...);
}

// Count the number of pieces of a certain type
template <typename... Pieces>
int Position::count(Piece pce, Pieces... pcs) const {
  return pieceCount[pce] + count(pcs...);
}

// Count the number of pieces of a certain type
template <typename... PieceTypes>
int Position::count(PieceType pt, PieceTypes... pts) const {
  return pieceCount[toPiece(WHITE, pt)] + pieceCount[toPiece(BLACK, pt)] +
         count(pts...);
}

// Get the slider blockers for a square
template <bool doMove>
void Position::castleRook(Square from, Square to, Square &rookFrom,
                          Square &rookTo) {
  bool kingSide = to > from;
  // Define the square that the rook is from
  rookFrom = kingSide ? H1 : A1;
  // Flip the rank if the side to move is black
  rookFrom = (sideToMove == BLACK) ? flipRank(rookFrom) : rookFrom;

  // Define the square that the rook is going to
  rookTo = kingSide ? F1 : D1;
  // Flip the rank if the side to move is black
  rookTo = (sideToMove == BLACK) ? flipRank(rookTo) : rookTo;

  // Move the rook
  if constexpr (doMove)
    // Move the rook
    movePiece(rookFrom, rookTo);
  else
    // Move the rook back
    movePiece(rookTo, rookFrom);
}

/******************************************\
|==========================================|
|             Class Functions              |
|==========================================|
\******************************************/

// State getter
inline BoardState *Position::state() const { return st; }

// Check if the position is in check
inline bool Position::isInCheck() const { return (st->checkMask != FULLBB); }

// Check if the position is draw
inline bool Position::isDraw(int ply) const {
  if (st->fiftyMove > 99 and !isInCheck())
    return true;
  return st->repetition and st->repetition < ply;
}

// Check if move is capture
inline bool Position::isCapture(Move move) const {
  return move.isValid() and getPiece(move.to());
}

// Returns bitboard for piece type
inline Bitboard Position::getPiecesBB(PieceType pt) const {
  return piecesBB[pt];
}

// Returns piece on square
inline Piece Position::getPiece(Square sq) const { return board[sq]; }

// Returns piece type on square
inline PieceType Position::getPieceType(Square sq) const {
  return pieceTypeOf(board[sq]);
}

// Returns the number of plies from the start
inline int Position::getPliesFromStart() const { return pliesFromStart; }

// Returns the occupany of one colour
inline Bitboard Position::getOccupiedBB() const {
  return occupiedBB[WHITE] | occupiedBB[BLACK];
}

// Returns the total occupancy
inline Bitboard Position::getOccupiedBB(Colour us) const {
  return occupiedBB[us];
}
// Returns side to move
inline Colour Position::getSideToMove() const { return sideToMove; }

// Returns the square the enPassant pawn is on
inline Square Position::getEnPassantTarget(Colour side) const {
  return st->enPassant + ((side == WHITE) ? N : S);
}

// Returns the non pawn material
inline int Position::getNonPawnMaterial() const {
  return st->nonPawnMaterial[WHITE] + st->nonPawnMaterial[BLACK];
}

// Returns the non pawn material for a colour
inline int Position::getNonPawnMaterial(Colour c) const {
  return st->nonPawnMaterial[c];
}
// Check if square is empty
inline bool Position::empty(Square sq) const {
  return getPiece(sq) == NO_PIECE;
}
// Check if a castling right is legal
inline bool Position::canCastle(Castling cr) const { return st->castling & cr; }

// inline Score Position::psq() const { return st->psq; }

// inline int Position::gamePhase() const { return st->gamePhase; }

// Returns pinners
inline Bitboard Position::pinners() const {
  return st->bishopPin & getPiecesBB(~sideToMove, BISHOP, QUEEN) |
         st->rookPin & getPiecesBB(~sideToMove, ROOK, QUEEN);
}
// Returns blockers for king
inline Bitboard Position::blockersForKing() const {
  return st->bishopPin & getOccupiedBB(sideToMove) |
         st->rookPin & getOccupiedBB(sideToMove);
}

#endif // POSITION_IMPL_HPP
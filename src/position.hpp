#ifndef POSITION_HPP
#pragma once
#define POSITION_HPP

#include <deque>
#include <memory>
#include <string>

#include "bitboard.hpp"
#include "defs.hpp"

namespace Maestro {

/******************************************\
|==========================================|
|            Useful fen strings            |
|==========================================|
\******************************************/

// By Code Monkey King
constexpr std::string_view emptyBoard = "8/8/8/8/8/8/8/8 b - - ";
constexpr std::string_view startPos =
    "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1 ";
constexpr std::string_view trickyPos =
    "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1 ";
constexpr std::string_view killerPos =
    "rnbqkb1r/pp1p1pPp/8/2p1pP2/1P1P4/3P3P/P1P1P3/RNBQKBNR w KQkq e6 0 1";
constexpr std::string_view cmkPos =
    "r2q1rk1/ppp2ppp/2n1bn2/2b1p3/3pP3/3P1NPP/PPP1NPB1/R1BQ1RK1 b - - 0 9 ";
constexpr std::string_view repetitions =
    "2r3k1/R7/8/1R6/8/8/P4KPP/8 w - - 0 40 ";

/******************************************\
|==========================================|
|               Board State                |
|          Used for unmaking moves         |
|==========================================|
\******************************************/

struct BoardState {

  BoardState copy(const BoardState &bs);

  // Copied when making new move
  Square enPassant;
  int plies;
  int fiftyMove;
  Value nonPawnMaterial[COLOUR_N];
  Castling castling = NO_CASTLE;
  Score psq;
  int gamePhase;

  // Not copied when making new move
  Key key;
  Key pawnKey;
  Piece captured = NO_PIECE;
  int repetition;
  Bitboard checkMask = FULLBB;
  Bitboard rookPin, bishopPin, kingBan, kingAttacks, available, attacked,
      pinned[COLOUR_N], pinners[COLOUR_N];
  bool enPassantPin = false;

  // Previous Board state
  BoardState *previous;
};

// Move history (Ideas from Stockfish)
using StateListPtr = std::unique_ptr<std::deque<BoardState>>;

/******************************************\
|==========================================|
|                 Position                 |
|==========================================|
\******************************************/

class Position {
public:
  Position() = default;
  Position(const Position &pos) = delete;
  Position &operator=(const Position &pos) = default;

  // Init Position
  void set(const std::string &fen, BoardState &state);

  std::string fen() const;

  // Init Keys
  Key initKey() const;
  Key initPawnKey() const;

  // Output
  void print() const;

  // Making and unmaking moves
  void makeMove(Move move, BoardState &state);
  void unmakeMove(Move move);
  void makeNullMove(BoardState &state);
  void unmakeNullMove();

  // Board State getters
  BoardState *state() const;
  Square enPassantTarget(Colour side) const;
  int nonPawnMaterial() const;
  int nonPawnMaterial(Colour c) const;
  Piece captured() const;
  int fiftyMove() const;
  Key key() const;
  Key pawnKey() const;
  Score psq() const;
  int gamePhase() const;
  Bitboard attacked() const;
  template <Colour us> Bitboard pinners() const;
  Bitboard pinners(Colour us) const;
  template <Colour us> Bitboard pinned() const;
  Bitboard pinned(Colour us) const;

  // Position getters
  int gamePlies() const;

  // Board helper functions
  bool empty(Square sq) const;
  bool canCastle(Castling cr) const;
  bool isInCheck() const;
  bool isDraw(int ply) const;
  bool isCapture(Move move) const;

  // Move helper functions
  PieceType capturedPiece(Move move) const;
  PieceType movedPieceType(Move move) const;
  Piece movedPiece(Move move) const;

  // Board piece bitboard getters
  Bitboard pieces(PieceType pt) const;
  template <typename... PieceTypes>
  Bitboard pieces(PieceType pt, PieceTypes... pts) const;
  template <typename... PieceTypes>
  Bitboard pieces(Colour us, PieceTypes... pts) const;

  // Get piece information from square
  Piece pieceOn(Square sq) const;
  PieceType pieceTypeOn(Square sq) const;
  template <PieceType pt> Square square(Colour us) const;
  template <PieceType pt> const Square *squares(Colour us) const;
  const Square *squares(Colour us, PieceType pt) const;

  // Piece count
  template <Piece pc> int count() const;
  template <PieceType pt> int count() const;
  int count(PieceType pt) const;
  int count(Piece pc) const;

  // Board occupancy
  Bitboard occupied() const;
  Bitboard occupied(Colour us) const;

  // Side to move
  Colour sideToMove() const;

  // Castling
  Castling castling() const;
  Castling castling(Colour us) const;

  // Slider blockers
  Bitboard sliderBlockers(Bitboard sliders, Square sq, Bitboard &pinners) const;

  // Move generation and legality helper functions
  Bitboard attackedByBB(Colour enemy) const;
  Bitboard sqAttackedByBB(Square sq, Bitboard occupied) const;
  bool isPseudoLegal(Move move) const;
  bool isLegal(Move move) const;
  Bitboard blockersForKing() const;

  // SEE function
  bool SEE(Move move, int threshold) const;

  // Gives check
  bool givesCheck(Move move) const;

  // Position has repeated
  bool hasRepeated() const;

  int _gamePlies;

private:
  void setState() const;
  // Piece manipulations
  void putPiece(Piece pce, Square sq);
  void popPiece(Square sq);
  void movePiece(Square from, Square to);
  template <bool doMove>
  void castleRook(Square from, Square to, Square &rookFrom, Square &rookTo);
  // Set state functions
  std::pair<int, int> initNonPawnMaterial() const;
  Score initPSQT() const;
  int initGamePhase() const;
  // Piece list
  Piece _board[SQ_N];
  int _pieceCount[PIECE_N];
  int _index[SQ_N];
  Square _pieceList[PIECE_N][16];
  // Bitboard representation
  Bitboard _piecesBB[PIECE_TYPE_N];
  Bitboard _occupiedBB[COLOUR_N];
  // Board state
  BoardState *st;
  Colour _sideToMove;
};

/******************************************\
|==========================================|
|            Template functions            |
|==========================================|
\******************************************/

// Get the pinners of a colour
template <Colour us> Bitboard Position::pinners() const {
  return st->pinners[us];
}

// Get the pinners of a colour
inline Bitboard Position::pinners(Colour us) const { return st->pinners[us]; }

// Get the pinned pieces of a colour
template <Colour us> Bitboard Position::pinned() const {
  return st->pinned[us];
}

// Get the pinned pieces of a colour
inline Bitboard Position::pinned(Colour us) const { return st->pinned[us]; }

// Get square of a piece type
template <PieceType pt> Square Position::square(Colour us) const {
  return _pieceList[toPiece(us, pt)][0];
}

// Get squares of a piece type
template <PieceType pt>
inline const Square *Position::squares(Colour us) const {
  return _pieceList[toPiece(us, pt)];
}

// Get the bitboard of pieces of certain types
template <typename... PieceTypes>
inline Bitboard Position::pieces(PieceType pt, PieceTypes... pts) const {
  return _piecesBB[pt] | pieces(pts...);
}

// Get the bitboard of pieces of certain types for a colour
template <typename... PieceTypes>
inline Bitboard Position::pieces(Colour us, PieceTypes... pts) const {
  return _occupiedBB[us] & pieces(pts...);
}

// U32 the number of pieces of a certain type
template <Piece pc> inline int Position::count() const {
  return _pieceCount[pc];
}

// U32 the number of pieces of a certain type
template <PieceType pt> inline int Position::count() const {
  return _pieceCount[toPiece(WHITE, pt)] + _pieceCount[toPiece(BLACK, pt)];
}

// Get the slider blockers for a square
template <bool doMove>
inline void Position::castleRook(Square from, Square to, Square &rookFrom,
                                 Square &rookTo) {
  bool kingSide = to > from;
  // Define the square that the rook is from
  rookFrom = kingSide ? H1 : A1;
  // Flip the rank if the side to move is black
  rookFrom = (_sideToMove == BLACK) ? flipRank(rookFrom) : rookFrom;

  // Define the square that the rook is going to
  rookTo = kingSide ? F1 : D1;
  // Flip the rank if the side to move is black
  rookTo = (_sideToMove == BLACK) ? flipRank(rookTo) : rookTo;

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

// Get attacked squares
inline Bitboard Position::attacked() const { return st->attacked; }

// Get squares of a piece type
inline const Square *Position::squares(Colour c, PieceType pt) const {
  return _pieceList[toPiece(c, pt)];
}

// Get previously captured piece
inline Piece Position::captured() const { return st->captured; }

// Get hash key
inline Key Position::key() const { return st->key; }

// Get pawn hash key
inline Key Position::pawnKey() const { return st->pawnKey; }

// Get fifty move counter
inline int Position::fiftyMove() const { return st->fiftyMove; }

// Check if the position is in check
inline bool Position::isInCheck() const { return (st->checkMask != FULLBB); }

// Check if move is capture
inline bool Position::isCapture(Move move) const {
  return move && (move.is<EN_PASSANT>() || pieceOn(move.to()));
}

inline PieceType Position::capturedPiece(Move move) const {
  return move.is<NORMAL>() ? pieceTypeOn(move.to()) : PAWN;
}

inline Piece Position::movedPiece(Move move) const {
  return pieceOn(move.from());
}

inline PieceType Position::movedPieceType(Move move) const {
  return pieceTypeOn(move.from());
}

// Returns bitboard for piece type
inline Bitboard Position::pieces(PieceType pt) const { return _piecesBB[pt]; }

// Returns piece on square
inline Piece Position::pieceOn(Square sq) const { return _board[sq]; }

// Returns piece type on square
inline PieceType Position::pieceTypeOn(Square sq) const {
  return pieceTypeOf(_board[sq]);
}

// Returns the number of plies from the start
inline int Position::gamePlies() const { return _gamePlies; }

// Returns the occupany of one colour
inline Bitboard Position::occupied() const {
  return _occupiedBB[WHITE] | _occupiedBB[BLACK];
}

// Returns the total occupancy
inline Bitboard Position::occupied(Colour us) const { return _occupiedBB[us]; }
// Returns side to move
inline Colour Position::sideToMove() const { return _sideToMove; }

// Returns castling rights
inline Castling Position::castling() const { return st->castling; }

// Returns castling rights for a side
inline Castling Position::castling(Colour us) const {
  return st->castling & ((us == WHITE) ? WHITE_SIDE : BLACK_SIDE);
}

// Returns the square the enPassant pawn is on
inline Square Position::enPassantTarget(Colour side) const {
  return st->enPassant + ((side == WHITE) ? N : S);
}

// Returns the non pawn material
inline int Position::nonPawnMaterial() const {
  return st->nonPawnMaterial[WHITE] + st->nonPawnMaterial[BLACK];
}

// Returns the non pawn material for a colour
inline int Position::nonPawnMaterial(Colour c) const {
  return st->nonPawnMaterial[c];
}
// Check if square is empty
inline bool Position::empty(Square sq) const { return pieceOn(sq) == NO_PIECE; }
// Check if a castling right is legal
inline bool Position::canCastle(Castling cr) const { return st->castling & cr; }

inline Score Position::psq() const { return st->psq; }

inline int Position::gamePhase() const { return st->gamePhase; }

inline int Position::count(PieceType pt) const {
  return _pieceCount[toPiece(WHITE, pt)] + _pieceCount[toPiece(BLACK, pt)];
}
inline int Position::count(Piece pc) const { return _pieceCount[pc]; }

// Returns blockers for king
inline Bitboard Position::blockersForKing() const {
  return st->bishopPin & occupied(_sideToMove) |
         st->rookPin & occupied(_sideToMove);
}

// Returns whether a position has repeated
inline bool Position::hasRepeated() const {
  BoardState *state = st;
  int end = std::min(_gamePlies, st->fiftyMove);
  while (end-- >= 4) {
    if (state->repetition)
      return true;

    state = state->previous;
  }
  return false;
}

} // namespace Maestro

#endif // POSITION_HPP
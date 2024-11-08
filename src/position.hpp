#ifndef POSITION_HPP
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

  BoardState &operator=(const BoardState &bs);

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

  // Previous pieceList state
  BoardState *previous;
};

// Move history (Ideas from Stockfish)
using StateListPtr = std::unique_ptr<std::deque<BoardState>>;

// Define thread for use in function
class Thread;

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
  void set(const std::string &fen, BoardState &state, Thread *th);

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

  // Boardstate getters
  BoardState *state() const;
  int getPliesFromStart() const;
  Square getEnPassantTarget(Colour side) const;
  int getNonPawnMaterial() const;
  int getNonPawnMaterial(Colour c) const;
  Piece getCaptured() const;
  int getFiftyMove() const;
  Key getKey() const;
  bool canCastle(Castling cr) const;
  bool isInCheck() const;
  bool isDraw(int ply) const;
  bool isCapture(Move move) const;
  PieceType captured(Move move) const;
  PieceType movedPieceType(Move move) const;
  Score psq() const;
  int gamePhase() const;
  bool empty(Square sq) const;

  // Board piece bitboard getters
  Bitboard getPiecesBB(PieceType pt) const;
  template <typename... PieceTypes>
  Bitboard getPiecesBB(PieceType pt, PieceTypes... pts) const;
  template <typename... PieceTypes>
  Bitboard getPiecesBB(Colour us, PieceTypes... pts) const;

  // Get piece information from square
  Piece getPiece(Square sq) const;
  PieceType getPieceType(Square sq) const;
  template <PieceType pt> Square square(Colour us) const;

  // Piece count
  template <Piece pc> int count() const;
  template <PieceType pt> int count() const;
  int count(PieceType pt) const;
  int count(Piece pc) const;

  // Board occupancy
  Bitboard getOccupiedBB() const;
  Bitboard getOccupiedBB(Colour us) const;

  // Side to move
  Colour getSideToMove() const;

  // Slider blockers
  Bitboard getSliderBlockers(Bitboard sliders, Square sq,
                             Bitboard &pinners) const;

  // Move generation and legality
  Bitboard attackedByBB(Colour enemy) const;
  Bitboard sqAttackedByBB(Square sq, Bitboard occupied) const;
  bool isPseudoLegal(Move move) const;
  bool isLegal(Move move) const;
  Bitboard pinners() const;
  Bitboard blockersForKing() const;

  // Static Exchange Evaluation
  bool SEE(Move move, int threshold = 0) const;

  // Gives check
  bool givesCheck(Move move) const;

  int pliesFromStart;

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
  Piece board[SQ_N];
  int pieceCount[PIECE_N];
  // Bitboard representation
  Bitboard piecesBB[PIECE_TYPE_N];
  Bitboard occupiedBB[COLOUR_N];
  // Board state
  BoardState *st;
  Colour sideToMove;

  // Thread
  Thread *thisThread;
};

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

// U32 the number of pieces of a certain type
template <Piece pc> int Position::count() const { return pieceCount[pc]; }

// U32 the number of pieces of a certain type
template <PieceType pt> int Position::count() const {
  return pieceCount[toPiece(WHITE, pt)] + pieceCount[toPiece(BLACK, pt)];
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

// Get previously captured piece
inline Piece Position::getCaptured() const { return st->captured; }

// Get hash key
inline Key Position::getKey() const { return st->key; }

// Get fifty move counter
inline int Position::getFiftyMove() const { return st->fiftyMove; }

// Check if the position is in check
inline bool Position::isInCheck() const { return (st->checkMask != FULLBB); }

// Check if move is capture
inline bool Position::isCapture(Move move) const {
  return move and getPiece(move.to());
}

inline PieceType Position::captured(Move move) const {
  return move.is<NORMAL>() ? getPieceType(move.to()) : PAWN;
}

inline PieceType Position::movedPieceType(Move move) const {
  return getPieceType(move.from());
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

inline Score Position::psq() const { return st->psq; }

inline int Position::gamePhase() const { return st->gamePhase; }

inline int Position::count(PieceType pt) const {
  return pieceCount[toPiece(WHITE, pt)] + pieceCount[toPiece(BLACK, pt)];
}
inline int Position::count(Piece pc) const { return pieceCount[pc]; }

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

} // namespace Maestro

#endif // POSITION_HPP
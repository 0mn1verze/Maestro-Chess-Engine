#ifndef POSITION_HPP
#define POSITION_HPP

#include <deque>
#include <memory>
#include <string>

#include "bitboard.hpp"
#include "defs.hpp"

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
|             Zobrist Hashing              |
|==========================================|
\******************************************/

// https://www.chessprogramming.org/Zobrist_Hashing

namespace Zobrist {
extern Key pieceSquareKeys[PIECE_N][SQ_N];
extern Key enPassantKeys[FILE_N];
extern Key castlingKeys[CASTLING_N];
extern Key sideKey;
} // namespace Zobrist

// Init zobrist hashing
void initZobrist();

/******************************************\
|==========================================|
|               Board State                |
|          Used for unmaking moves         |
|==========================================|
\******************************************/

struct BoardState {
  // Copied when making new move
  Square enPassant;
  int plies;
  int fiftyMove;
  Value nonPawnMaterial[COLOUR_N];
  Castling castling = NO_CASTLE;

  // Not copied when making new move
  Key key;
  Piece captured = NO_PIECE;
  int repetition;
  Move move = Move::none();
  Bitboard checkMask = FULLBB;
  Bitboard rookPin, bishopPin, kingBan, kingAttacks, available, attacked,
      blockersForKing[COLOUR_N], pinners[COLOUR_N];
  bool enPassantPin = false;

  // Previous pieceList state
  BoardState *previous;
};

// Move history (Ideas from Stockfish)
using StateList = std::unique_ptr<std::deque<BoardState>>;

/******************************************\
|==========================================|
|                 Position                 |
|==========================================|
\******************************************/

class Position {
public:
  // Constructors
  Position() = default;
  Position(const Position &pos) = delete;
  Position &operator=(const Position &pos) = default;

  // Init Position
  void set(const std::string &fen, BoardState &state);
  void setState() const;
  std::string fen() const;

  // Output
  void print() const;

  // Making and unmaking moves
  void makeMove(Move move, BoardState &state);
  void unmakeMove();
  void makeNullMove(BoardState &state);
  void unmakeNullMove();

  // Boardstate getters
  BoardState *state() const;
  int getPliesFromStart() const;
  Square getEnPassantTarget(Colour side) const;
  int getNonPawnMaterial() const;
  int getNonPawnMaterial(Colour c) const;
  Castling castling(Colour c) const;
  bool canCastle(Castling cr) const;
  bool isInCheck() const;
  bool isDraw(int ply) const;
  bool isCapture(Move move) const;

  // Board getters
  Bitboard getPiecesBB(PieceType pt) const;
  template <typename... PieceTypes>
  Bitboard getPiecesBB(PieceType pt, PieceTypes... pts) const;
  template <typename... PieceTypes>
  Bitboard getPiecesBB(Colour us, PieceTypes... pts) const;
  Piece getPiece(Square sq) const;
  PieceType getPieceType(Square sq) const;
  template <PieceType pt> Square square(Colour us) const;
  int getPieceCount(Piece pce) const;
  Bitboard getOccupiedBB() const;
  Bitboard getOccupiedBB(Colour us) const;
  bool empty(Square sq) const;
  Colour getSideToMove() const;
  Bitboard getSliderBlockers(Bitboard sliders, Square sq,
                             Bitboard &pinners) const;

  // Piece manipulations
  void putPiece(Piece pce, Square sq);
  void popPiece(Square sq);
  void movePiece(Square from, Square to);
  template <bool doMove>
  void castleRook(Square from, Square to, Square &rookFrom, Square &rookTo);

  // Move generation and legality
  Bitboard attackedByBB(Colour enemy) const;
  Bitboard sqAttackedByBB(Square sq, Bitboard occupied) const;
  bool isLegal(Move move) const;
  bool SEE(Move move, Value threshold) const;

private:
  // Piece list
  Piece board[SQ_N];
  int pieceCount[PIECE_N];
  // Bitboard representation
  Bitboard piecesBB[PIECE_TYPE_N];
  Bitboard occupiedBB[COLOUR_N + 1];
  // Board state
  BoardState *st;
  Colour sideToMove;
  int pliesFromStart;
};

template <PieceType pt> Square Position::square(Colour us) const {
  return getLSB(getPiecesBB(us, pt));
}

template <typename... PieceTypes>
Bitboard Position::getPiecesBB(PieceType pt, PieceTypes... pts) const {
  return piecesBB[pt] | getPiecesBB(pts...);
}
template <typename... PieceTypes>
Bitboard Position::getPiecesBB(Colour us, PieceTypes... pts) const {
  return occupiedBB[us] & getPiecesBB(pts...);
}

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
#endif // POSITION_HPP
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
  U16 nonPawnMaterial[COLOUR_N];
  Castling castling = NO_CASTLE;

  // Not copied when making new move
  Key key;
  Key pawnKey;
  Piece captured = NO_PIECE;
  int repetition;
  Move move = Move::none();
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
  template <typename... Pieces> int count(Piece pce, Pieces... pcs) const;
  template <typename... PieceTypes>
  int count(PieceType pt, PieceTypes... pts) const;

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
  bool isLegal(Move move) const;
  Bitboard pinners() const;
  Bitboard blockersForKing() const;

  int pliesFromStart;

private:
  void setState() const;
  // Piece manipulations
  void putPiece(Piece pce, Square sq);
  void popPiece(Square sq);
  void movePiece(Square from, Square to);
  template <bool doMove>
  void castleRook(Square from, Square to, Square &rookFrom, Square &rookTo);
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

#include "position.ipp"

#endif // POSITION_HPP
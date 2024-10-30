#include <algorithm>
#include <cstring>
#include <iostream>
#include <sstream>

#include "bitboard.hpp"
#include "defs.hpp"
#include "movegen.hpp"
#include "position.hpp"
#include "utils.hpp"

/******************************************\
|==========================================|
|            Piece Manipulation            |
|==========================================|
\******************************************/

// Put piece on square
void Position::putPiece(Piece piece, Square sq) {
  // Put piece in piece list
  board[sq] = piece;
  // Update piece bitboards
  piecesBB[ALL_PIECES] |= piecesBB[pieceTypeOf(piece)] |= sq;
  // Update occupancy bitboards
  occupiedBB[colourOf(piece)] |= sq;
  occupiedBB[BOTH] |= sq;
  // Update piece count list
  pieceCount[piece]++;
  pieceCount[toPiece(colourOf(piece), ALL_PIECES)]++;
}

// Remove piece on square
void Position::popPiece(Square sq) {
  // Find piece on square
  Piece piece = board[sq];
  // Update piece bitboards
  piecesBB[ALL_PIECES] ^= sq;
  piecesBB[pieceTypeOf(piece)] ^= sq;
  // Update occupancy bitboards
  occupiedBB[colourOf(piece)] ^= sq;
  occupiedBB[BOTH] ^= sq;
  // Update piece list at square
  board[sq] = NO_PIECE;
  // Update piece count
  pieceCount[piece]--;
  pieceCount[toPiece(colourOf(piece), ALL_PIECES)]--;
}

// Move piece between two squares without updating piece counts (Quicker)
void Position::movePiece(Square from, Square to) {
  // Find piece on square
  Piece piece = board[from];
  // Update piece bitboards
  piecesBB[ALL_PIECES] ^= from | to;
  piecesBB[pieceTypeOf(piece)] ^= from | to;
  // Update occupancy bitboards
  occupiedBB[colourOf(piece)] ^= from | to;
  occupiedBB[BOTH] ^= from | to;
  // Update piece list
  board[from] = NO_PIECE;
  board[to] = piece;
}

/******************************************\
|==========================================|
|         Position Set Up Functions        |
|==========================================|
\******************************************/

// Returns ASCII representation of the board
void Position::print() const {
  // Row seperator
  const std::string seperator = "\n  +---+---+---+---+---+---+---+---+\n";

  // Result string
  std::cout << seperator;

  // Print bitboard
  for (Rank rank = RANK_8; rank >= RANK_1; --rank) {

    // Print rank number
    std::cout << rank + 1 << " ";

    for (File file = FILE_A; file <= FILE_H; ++file) {
      // Get square
      Square square = toSquare(file, rank);
      // Print bit on that square
      std::cout << "| " << piece2Str(getPiece(square)) << " ";
    }

    // Print bit on that square
    std::cout << "|" + seperator;
  }

  // Print files
  std::cout << "    a   b   c   d   e   f   g   h\n\n";

  // Print side to move
  std::cout << "Side to move: " << (sideToMove == WHITE ? "White" : "Black")
            << "\n";

  // Print castling rights
  std::cout << "Castling rights: ";
  // Print individual castling rights (Either KQkq or -)
  if (st->castling & WK_SIDE)
    std::cout << "k";
  else
    std::cout << "-";
  if (st->castling & WQ_SIDE)
    std::cout << "Q";
  else
    std::cout << "-";
  if (st->castling & BK_SIDE)
    std::cout << "k";
  else
    std::cout << "-";
  if (st->castling & BQ_SIDE)
    std::cout << "q";
  else
    std::cout << "-";

  std::cout << std::endl;
  // Print Enpassant square
  std::cout << "Enpassant Square: ";
  if (st->enPassant != NO_SQ)
    std::cout << sq2Str(st->enPassant) << std::endl;
  else
    std::cout << "None" << std::endl;

  // Print hash key
  std::cout << "Hash Key: " << std::hex << st->key << std::dec << std::endl;
}

// Set the position based on fen string
void Position::set(const std::string &fen, BoardState &state) {
  // Initialize square and piece index
  Square square = A8;
  size_t pieceIdx;
  // Create token to store tokens in the input stream
  unsigned char token;
  // If fen is empty, return empty position
  if (fen.empty())
    return;
  // Create input stream with fen strng to parse it
  std::istringstream is(fen);

  // Reset board state
  std::memset(this, 0, sizeof(Position));

  // Reset new state
  state = {};
  // Set board state
  this->st = &state;

  // Set no skip whitespace flag to detect white spaces
  is >> std::noskipws;

  // Parse board pieces (Loop while there are non-space tokens)
  while ((is >> token) && !isspace(token)) {

    // If token is '/' continue to next token
    if (token == '/')
      square += SS;

    // If token is a number, skip that many squares
    else if (isdigit(token))
      square += E * (token - '0');

    // If token is a piece, put it on the square
    else if ((pieceIdx = pieceToChar.find(token)) != std::string::npos) {
      // Put piece on the square
      putPiece(Piece(pieceIdx), square);
      // Move to next square
      ++square;
    }
  }

  // Parse side to move
  is >> token;
  sideToMove = (token == 'w') ? WHITE : BLACK;
  is >> token;

  // Parse castling rights
  while ((is >> token) && !isspace(token)) {
    switch (token) {
    case 'K':
      st->castling |= WK_SIDE;
      break;
    case 'Q':
      st->castling |= WQ_SIDE;
      break;
    case 'k':
      st->castling |= BK_SIDE;
      break;
    case 'q':
      st->castling |= BQ_SIDE;
      break;
    default:
      break;
    }
  }

  // Parse enpassant square
  unsigned char file, rank;

  if (((is >> file) && (file >= 'a' && file <= 'h')) &&
      ((is >> rank) && (rank == (sideToMove == WHITE ? '6' : '3'))))
    // Set enpassant square
    st->enPassant = toSquare(File(file - 'a'), Rank(rank - '1'));
  else
    // If enpassant square is not set, set it to NO_SQ
    st->enPassant = NO_SQ;

  // Parse halfmove clock
  is >> std::skipws >> st->fiftyMove;

  // initialise hash key
  st->key = initKey();

  // init pawn key
  st->pawnKey = initPawnKey();

  Bitboard pawns = getPiecesBB(~sideToMove, PAWN);
  Bitboard knights = getPiecesBB(~sideToMove, KNIGHT);

  Bitboard attacks;

  while (pawns) {
    Square sq = popLSB(pawns);
    if (~sideToMove == WHITE) {
      attacks = pawnAttacksBB<WHITE>(sq);
    } else {
      attacks = pawnAttacksBB<BLACK>(sq);
    }

    if (attacks & getPiecesBB(sideToMove, KING)) {
      st->checkMask = squareBB(sq);
    }
  }

  while (knights) {
    Square sq = popLSB(knights);
    attacks = attacksBB<KNIGHT>(sq, EMPTYBB);
    if (attacks & getPiecesBB(sideToMove, KING)) {
      st->checkMask = squareBB(sq);
    }
  }

  // Refresh masks
  refreshMasks(*this);
}

void Position::setState(BoardState &state) { st = &state; }

Key Position::initKey() const {
  // Initialize hash key
  Key key = 0ULL;
  // Loop through all squares
  for (Square square = A1; square <= H8; ++square) {
    // Get piece on the square
    Piece piece = getPiece(square);
    // If there is a piece on the square, update hash key
    if (piece != NO_PIECE)
      key ^= Zobrist::pieceSquareKeys[piece][square];
  }
  // Update hash key with side to move
  if (sideToMove == BLACK)
    key ^= Zobrist::sideKey;
  // Update hash key with castling rights
  key ^= Zobrist::castlingKeys[st->castling];
  // Update hash key with enpassant square
  if (st->enPassant != NO_SQ)
    key ^= Zobrist::enPassantKeys[fileOf(st->enPassant)];

  return key;
}

Key Position::initPawnKey() const {
  // Initialize hash key
  Key key = 0ULL;
  // Loop through all squares
  for (Square square = A1; square <= H8; ++square) {
    // Get piece on the square
    Piece piece = getPiece(square);
    // If there is a pawn on the square, update hash key
    if (pieceTypeOf(piece) == PAWN)
      key ^= Zobrist::pieceSquareKeys[piece][square];
  }
  // Return hash key
  return key;
}

/******************************************\
|==========================================|
|             Board functions              |
|==========================================|
\******************************************/

Bitboard Position::sqAttackedByBB(Square sq, Bitboard occupied) const {
  return (pawnAttacksBB<BLACK>(sq) & getPiecesBB(WHITE, PAWN)) |
         (pawnAttacksBB<WHITE>(sq) & getPiecesBB(BLACK, PAWN)) |
         (attacksBB<KNIGHT>(sq, occupied) & getPiecesBB(KNIGHT)) |
         (attacksBB<BISHOP>(sq, occupied) & getPiecesBB(BISHOP, QUEEN)) |
         (attacksBB<ROOK>(sq, occupied) & getPiecesBB(ROOK, QUEEN)) |
         (attacksBB<KING>(sq, occupied) & getPiecesBB(KING));
}

// Private functions
Bitboard Position::attackedByBB(Colour enemy) const {
  Square attacker;
  Bitboard attacks = EMPTYBB;

  // Knight attacks
  Bitboard knights = getPiecesBB(enemy, KNIGHT);
  while (knights) {
    attacker = popLSB(knights);
    attacks |= attacksBB<KNIGHT>(attacker, EMPTYBB);
  }

  // Pawn attacks
  Bitboard pawns = getPiecesBB(enemy, PAWN);
  attacks |= (enemy == WHITE) ? pawnAttacksBB<WHITE>(pawns)
                              : pawnAttacksBB<BLACK>(pawns);

  // King attacks
  Bitboard kings = getPiecesBB(enemy, KING);
  attacks |= attacksBB<KING>(popLSB(kings), EMPTYBB);

  // Bishop and Queen attacks
  Bitboard bishops = getPiecesBB(enemy, BISHOP, QUEEN);
  while (bishops) {
    attacker = popLSB(bishops);
    attacks |= attacksBB<BISHOP>(attacker, getOccupiedBB());
  }

  // Rook and Queen attacks
  Bitboard rooks = getPiecesBB(enemy, ROOK, QUEEN);
  while (rooks) {
    attacker = popLSB(rooks);
    attacks |= attacksBB<ROOK>(attacker, getOccupiedBB());
  }

  return attacks;
}

Bitboard Position::getSliderBlockers(Bitboard sliders, Square sq,
                                     Bitboard &pinners) const {
  Bitboard blockers = EMPTYBB;
  pinners = 0;

  Bitboard snipers = ((attacksBB<BISHOP>(sq) & getPiecesBB(BISHOP, QUEEN)) |
                      (attacksBB<ROOK>(sq) & getPiecesBB(ROOK, QUEEN))) &
                     sliders;
  Bitboard occupancy = getOccupiedBB() ^ snipers;

  while (snipers) {
    Square sniperSq = popLSB(snipers);
    Bitboard b = betweenBB[sq][sniperSq] & occupancy;

    if (b && !moreThanOne(b)) {
      blockers |= b;
      if (b & getOccupiedBB(colourOf(getPiece(sq)))) {
        pinners |= sniperSq;
      }
    }
  }

  return blockers;
}

bool Position::isDraw(int ply) const {
  if (st->fiftyMove > 99)
    return true;
  return st->repetition and st->repetition < ply;
}

bool Position::isCapture(Move move) const { return getPiece(move.to()); }

// Colour Position::getSideToMove() const { return sideToMove; }
// Square Position::getEnPassantTarget(Colour side) const {
//   return st->enPassant + ((side == WHITE) ? N : S);
// }

// Square Position::kingSquare(Colour side) const {
//   return getLSB(getPiecesBB(side, KING));
// }
// bool Position::isOnSemiOpenFile(Colour side, Square sq) const {
//   return !(getPiecesBB(side, PAWN) & fileBB(sq));
// }
// Castling Position::castling(Colour c) const {
//   return Castling(st->castling & (c == WHITE ? WHITE_SIDE : BLACK_SIDE));
// }

/******************************************\
|==========================================|
|               Make/Unmake                |
|==========================================|
\******************************************/

void Position::makeMove(Move move, BoardState &state) {
  // Reset new state
  state = {};
  state = *st;
  // std::memcpy(&state, st, sizeof(BoardState));

  // Get Hash Key (And change sides)
  Key hashKey = st->key ^ Zobrist::sideKey;
  Key pawnKey = st->pawnKey;

  state.previous = st;
  st = &state;

  ++st->fiftyMove;
  ++st->plies;
  ++pliesFromStart;

  // Get move variables
  const Colour side = sideToMove;
  const Colour enemy = ~side;
  const Square from = move.from();
  const Square to = move.to();
  const Piece piece = getPiece(from);
  const Piece cap = move.is<EN_PASSANT>() ? toPiece(enemy, PAWN) : getPiece(to);

  // Handle castling
  if (move.is<CASTLE>()) {
    // Move rook
    Square rookFrom, rookTo;
    Piece rook = toPiece(side, ROOK);
    castleRook<true>(from, to, rookFrom, rookTo);
    // Update hash key
    hashKey ^= Zobrist::pieceSquareKeys[rook][rookFrom] ^
               Zobrist::pieceSquareKeys[rook][rookTo];
  }

  // Handle captures
  if (cap != NO_PIECE) {
    Square capSq = to;
    // Handle enpassant
    if (move.is<EN_PASSANT>()) {
      // Change captured square to the enpassant square
      capSq += (sideToMove == WHITE ? S : N);
    }
    // Update board by removing piece on the destination
    popPiece(capSq);
    occupiedBB[enemy] &= ~squareBB(capSq);
    // Update hash key
    hashKey ^= Zobrist::pieceSquareKeys[cap][capSq];

    // Update board state to undo move
    st->captured = cap;
  } else
    st->captured = NO_PIECE;

  // Reset enpassant square
  if (st->enPassant != NO_SQ) {
    // Update hash key
    hashKey ^= Zobrist::enPassantKeys[fileOf(st->enPassant)];
    // Remove enpassant square
    st->enPassant = NO_SQ;
  }
  // Enpassant Square update
  if (std::abs(to - from) == 16 && pieceTypeOf(piece) == PAWN) [[likely]] {
    st->enPassant = from + ((sideToMove == WHITE) ? N : S);
    // Update hash key
    hashKey ^= Zobrist::enPassantKeys[fileOf(st->enPassant)];
  }

  // Update board by moving piece to the destination
  movePiece(from, to);
  // Update hash key
  hashKey ^= Zobrist::pieceSquareKeys[piece][from] ^
             Zobrist::pieceSquareKeys[piece][to];

  if (pieceTypeOf(piece) == PAWN) {
    // Handle promotions
    if (move.is<PROMOTION>()) {
      Piece promotedTo = toPiece(side, move.promoted());
      // Swap the piece on the promotion square
      popPiece(to);
      putPiece(promotedTo, to);

      // Update check mask for promotions
      if (pieceTypeOf(promotedTo) == KNIGHT &&
          attacksBB<KNIGHT>(to, EMPTYBB) & getPiecesBB(enemy, KING))
        st->checkMask = squareBB(to);

      // Update hash key
      hashKey ^= Zobrist::pieceSquareKeys[piece][to] ^
                 Zobrist::pieceSquareKeys[promotedTo][to];

      pawnKey ^= Zobrist::pieceSquareKeys[piece][to];
    }
    // Update fifty move rule
    st->fiftyMove = 0;

    pawnKey ^= Zobrist::pieceSquareKeys[piece][from] ^
               Zobrist::pieceSquareKeys[piece][to];

    // Handle pawn checks to update check mask
    Bitboard pawnAttack =
        (side == WHITE) ? pawnAttacksBB<WHITE>(to) : pawnAttacksBB<BLACK>(to);
    if (pawnAttack & getPiecesBB(enemy, KING))
      st->checkMask = squareBB(to);
  }

  // Update checkmask for piece moves
  if (pieceTypeOf(piece) == KNIGHT &&
      attacksBB<KNIGHT>(to, EMPTYBB) & getPiecesBB(enemy, KING))
    st->checkMask = squareBB(to);

  // Update hash key (Remove previous castling rights)
  hashKey ^= Zobrist::castlingKeys[st->castling];
  // Update castling flag (By Code Monkey King)
  st->castling &= castlingRights[from] & castlingRights[to];
  // Update hash key (Add new castling rights)
  hashKey ^= Zobrist::castlingKeys[st->castling];

  // Update side to move
  sideToMove = ~sideToMove;

  // Update state hash key
  st->key = hashKey;

  // Update state pawn hash key
  st->pawnKey = pawnKey;

  // Update move
  st->move = move;

  // Update repetition
  st->repetition = 0;

  int end = std::min(st->plies, st->fiftyMove);
  if (end >= 4) {
    // Go back 4 ply's (2 moves) to check for repetitions
    BoardState *prev = st->previous->previous;
    for (int i = 4; i <= end; i += 2) {
      prev = prev->previous->previous;
      // Check for repetition, if the previous position is also repeated then
      // set the repetition state to negative
      if (prev->key == st->key) {
        st->repetition = (prev->repetition ? -i : i);
        break;
      }
    }
  }

  refreshMasks(*this);
}

void Position::unmakeMove() {
  Move move = st->move;
  // Restore side to move
  sideToMove = ~sideToMove;

  // Get move variables
  const Colour side = sideToMove;
  const Colour enemy = ~side;
  Square from = move.from();
  Square to = move.to();
  Piece piece = getPiece(to);

  // Restore promotions
  if (move.is<PROMOTION>()) {
    popPiece(to);
    putPiece(toPiece(side, PAWN), to);
  }

  // Restore castling
  if (move.is<CASTLE>()) {
    Square rookFrom, rookTo;
    castleRook<false>(from, to, rookFrom, rookTo);
  }

  // Restore move
  movePiece(to, from);

  // Restore captures
  if (st->captured != NO_PIECE) {
    Square capSq = to;
    // Restore enpassant
    if (move.is<EN_PASSANT>()) {
      capSq += (sideToMove ? N : S);
    }

    // Restore captured piece
    putPiece(st->captured, capSq);
  }

  st = st->previous;
  --pliesFromStart;
}

void Position::makeNullMove(BoardState &state) {
  state = {};
  // Copy current board state to new state partially
  std::memcpy(&state, st, sizeof(BoardState));

  state.previous = st;
  st = &state;

  if (st->enPassant != NO_SQ) {
    st->key ^= Zobrist::enPassantKeys[fileOf(st->enPassant)];
    st->enPassant = NO_SQ;
  }

  st->key ^= Zobrist::sideKey;
  ++st->fiftyMove;
  st->plies = 0;

  // Update side to move
  sideToMove = ~sideToMove;

  st->move = Move::null();

  // Update repetition
  st->repetition = 0;

  refreshMasks(*this);
}

void Position::unmakeNullMove() {

  // Restore board state
  st = st->previous;

  // Restore side to move
  sideToMove = ~sideToMove;
}

bool Position::isLegal(Move move) const {
  Colour us = sideToMove;
  Square from = move.from();
  Square to = move.to();

  if (move == Move::none() || move == Move::null() ||
      getPiece(from) == NO_PIECE || colourOf(getPiece(from)) != us ||
      getPieceType(to) == KING ||
      (colourOf(getPiece(to)) == us and getPiece(to) != NO_PIECE)) {
    return false;
  }

  if (move.is<PROMOTION>() and move.promoted() == NO_PIECE_TYPE)
    return false;

  if (move.is<EN_PASSANT>() and (st->enPassantPin || st->enPassant != to)) {
    return false;
  }

  if (move.is<CASTLE>()) {
    Bitboard castlingSquares = us == WHITE ? (to > from ? F1 | G1 : D1 | C1)
                                           : (to > from ? F8 | G8 : D8 | C8);
    Square rookSquare =
        to > from ? relativeSquare(us, H1) : relativeSquare(us, A1);

    if (((getOccupiedBB() | st->kingBan) & castlingSquares) ||
        kingSquare(us) != from || getPiece(rookSquare) != toPiece(us, ROOK) ||
        isInCheck()) {
      return false;
    }
  }

  bool isCapture, isSinglePush, isDoublePush;
  Bitboard attacks;
  switch (getPieceType(from)) {
  case PAWN:
    if (move.is<EN_PASSANT>())
      break;
    attacks =
        (us == WHITE) ? pawnAttacksBB<WHITE>(from) : pawnAttacksBB<BLACK>(from);
    isCapture = bool(attacks & getOccupiedBB(~us) & to);
    isSinglePush =
        bool((from + pawnPush(us) == to) and !(getOccupiedBB() & to));
    isDoublePush = bool((from + 2 * pawnPush(us) == to) and
                        (rankOf(from) == relativeRank(us, RANK_2)) and
                        !(getOccupiedBB() & to) and
                        !(getOccupiedBB() & (to - pawnPush(us))));

    if (!isCapture and !isSinglePush and !isDoublePush)
      return false;
    break;
  case KING:
    if ((st->kingBan & to) || (distance(from, to) > 1 and !move.is<CASTLE>()))
      return false;
    break;
  case KNIGHT:
    if (!(attacksBB<KNIGHT>(from, getOccupiedBB()) & to))

      return false;
    break;
  case BISHOP:
    if (!(attacksBB<BISHOP>(from, getOccupiedBB()) & to))
      return false;
    break;
  case ROOK:
    if (!(attacksBB<ROOK>(from, getOccupiedBB()) & to))
      return false;
    break;
  case QUEEN:
    if (!(attacksBB<QUEEN>(from, getOccupiedBB()) & to))
      return false;
    break;
  }

  if (getPieceType(from) != KING and isInCheck() and !(st->checkMask & to))
    return false;

  return !(st->pinned[us] & from) || aligned(from, to, kingSquare(us));
}
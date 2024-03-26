#include <algorithm>
#include <cstring>
#include <iostream>
#include <sstream>

#include "bitboard.hpp"
#include "defs.hpp"
#include "eval.hpp"
#include "movegen.hpp"
#include "position.hpp"
#include "utils.hpp"

/******************************************\
|==========================================|
|             Zobrist Hashing              |
|==========================================|
\******************************************/

// https://www.chessprogramming.org/Zobrist_Hashing

namespace Zobrist {
Key pieceSquareKeys[PIECE_N][SQ_N]{};
Key enPassantKeys[FILE_N]{};
Key castlingKeys[CASTLING_N]{};
Key sideKey{};
} // namespace Zobrist

// Initialize zobrist keys
void initZobrist() {
  // Initialize piece square keys
  for (Piece pce : {wP, wN, wB, wR, wQ, wK, bP, bN, bB, bR, bR, bQ, bK})
    for (Square sq = A1; sq <= H8; ++sq)
      Zobrist::pieceSquareKeys[pce][sq] = getRandom<Key>();

  // Initialize enpassant keys
  for (File file = FILE_A; file <= FILE_H; ++file)
    Zobrist::enPassantKeys[file] = getRandom<Key>();

  // Initialize castling keys
  for (Castling c = NO_CASTLE; c <= ANY_SIDE; ++c)
    Zobrist::castlingKeys[c] = getRandom<Key>();

  // Initialize side key
  Zobrist::sideKey = getRandom<Key>();
}

/******************************************\
|==========================================|
|             Class Functions              |
|==========================================|
\******************************************/

BoardState *Position::state() const { return st; }

bool Position::isInCheck() const { return (st->checkMask != FULLBB); }

bool Position::isDraw(int ply) const {
  if (st->fiftyMove > 99)
    return true;
  return st->repetition and st->repetition < ply;
}

bool Position::isCapture(Move move) const { return getPiece(move.to()); }

Bitboard Position::getPiecesBB(PieceType pt) const { return piecesBB[pt]; }

Piece Position::getPiece(Square sq) const { return board[sq]; }

PieceType Position::getPieceType(Square sq) const {
  return toPieceType(board[sq]);
}

int Position::getPliesFromStart() const { return pliesFromStart; }

int Position::getPieceCount(Piece pce) const { return pieceCount[pce]; }

Bitboard Position::getOccupiedBB() const { return occupiedBB[BOTH]; }

Bitboard Position::getOccupiedBB(Colour us) const { return occupiedBB[us]; }

Colour Position::getSideToMove() const { return sideToMove; }

Square Position::getEnPassantTarget(Colour side) const {
  return st->enPassant + ((side == WHITE) ? N : S);
}

int Position::getNonPawnMaterial() const {
  return st->nonPawnMaterial[WHITE] + st->nonPawnMaterial[BLACK];
}

int Position::getNonPawnMaterial(Colour c) const {
  return st->nonPawnMaterial[c];
}

Castling Position::castling(Colour c) const {
  return Castling(st->castling & (c == WHITE ? WHITE_SIDE : BLACK_SIDE));
}

bool Position::empty(Square sq) const { return getPiece(sq) == NO_PIECE; }

bool Position::canCastle(Castling cr) const { return st->castling & cr; }

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
  piecesBB[toPieceType(piece)] |= sq;
  // Update occupancy bitboards
  occupiedBB[toColour(piece)] |= sq;
  occupiedBB[BOTH] |= sq;
  // Update piece count list
  pieceCount[piece]++;
  pieceCount[toPiece(toColour(piece), ALL_PIECES)]++;
}

// Remove piece on square
void Position::popPiece(Square sq) {
  // Find piece on square
  Piece piece = board[sq];
  // Update piece bitboards
  piecesBB[toPieceType(piece)] ^= sq;
  // Update occupancy bitboards
  occupiedBB[toColour(piece)] ^= sq;
  occupiedBB[BOTH] ^= sq;
  // Update piece list at square
  board[sq] = NO_PIECE;
  // Update piece count
  pieceCount[piece]--;
  pieceCount[toPiece(toColour(piece), ALL_PIECES)]--;
}

// Move piece between two squares without updating piece counts (Quicker)
void Position::movePiece(Square from, Square to) {
  // Find piece on square
  Piece piece = board[from];
  // Update piece bitboards
  piecesBB[toPieceType(piece)] ^= from | to;
  // Update occupancy bitboards
  occupiedBB[toColour(piece)] ^= from | to;
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
  if (st->castling & WKCA)
    std::cout << "k";
  else
    std::cout << "-";
  if (st->castling & WQCA)
    std::cout << "Q";
  else
    std::cout << "-";
  if (st->castling & BKCA)
    std::cout << "k";
  else
    std::cout << "-";
  if (st->castling & BQCA)
    std::cout << "q";
  else
    std::cout << "-";

  std::cout << "\n";

  std::cout << "Fen: " << fen() << std::endl;

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
      st->castling |= WKCA;
      break;
    case 'Q':
      st->castling |= WQCA;
      break;
    case 'k':
      st->castling |= BKCA;
      break;
    case 'q':
      st->castling |= BQCA;
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

  setState();
}

void Position::setState() const {
  // Initialize hash key
  st->key = 0ULL;
  // Loop through all squares
  for (Square square = A1; square <= H8; ++square) {
    // Get piece on the square
    Piece piece = getPiece(square);
    // If there is a piece on the square, update hash key
    if (piece != NO_PIECE)
      st->key ^= Zobrist::pieceSquareKeys[piece][square];
  }
  // Update hash key with side to move
  if (sideToMove == BLACK)
    st->key ^= Zobrist::sideKey;
  // Update hash key with castling rights
  st->key ^= Zobrist::castlingKeys[st->castling];
  // Update hash key with enpassant square
  if (st->enPassant != NO_SQ)
    st->key ^= Zobrist::enPassantKeys[fileOf(st->enPassant)];

  // Initialize non pawn material
  st->nonPawnMaterial[WHITE] = st->nonPawnMaterial[BLACK] = 0;
  // Loop through all pieces
  for (PieceType pt = KNIGHT; pt <= KING; ++pt) {
    // Update non pawn material
    st->nonPawnMaterial[WHITE] +=
        PieceValue[toPiece(WHITE, pt)] * pieceCount[toPiece(WHITE, pt)];
    st->nonPawnMaterial[BLACK] +=
        PieceValue[toPiece(BLACK, pt)] * pieceCount[toPiece(BLACK, pt)];
  }

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

std::string Position::fen() const {
  int emptyCount;
  std::ostringstream os;

  for (Rank r = RANK_8; r >= RANK_1; --r) {
    for (File f = FILE_A; f <= FILE_H; ++f) {

      for (emptyCount = 0; f <= FILE_H and empty(toSquare(f, r)); ++f)
        ++emptyCount;

      if (emptyCount)
        os << emptyCount;

      if (f <= FILE_H) {
        os << piece2Char(getPiece(toSquare(f, r)));
      }
    }

    if (r > RANK_1)
      os << '/';
  }

  os << (sideToMove == WHITE ? " w " : " b ");

  if (canCastle(WKCA))
    os << 'K';

  if (canCastle(WQCA))
    os << 'Q';

  if (canCastle(BKCA))
    os << 'k';

  if (canCastle(BQCA))
    os << 'q';

  if (!canCastle(ANY_SIDE))
    os << '-';

  os << (st->enPassant == NO_SQ ? " - " : " " + sq2Str(st->enPassant) + " ")
     << st->fiftyMove << " "
     << 1 + (pliesFromStart - (sideToMove == BLACK)) / 2;

  return os.str();
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
      if (b & getOccupiedBB(toColour(getPiece(sq)))) {
        pinners |= sniperSq;
      }
    }
  }

  return blockers;
}

/******************************************\
|==========================================|
|               Make/Unmake                |
|==========================================|
\******************************************/

void Position::makeMove(Move move, BoardState &state) {
  // Reset new state
  state = {};
  // Copy current board state to new state partially
  std::memcpy(&state, st, offsetof(BoardState, key));

  // Get Hash Key (And change sides)
  Key hashKey = st->key ^ Zobrist::sideKey;

  state.previous = st;
  st = &state;

  ++st->fiftyMove;
  ++st->plies;
  ++pliesFromStart;

  st->nnueData.accumulator.computedAccumulation = false;
  auto &dp = st->nnueData.dirtyPiece;
  dp.dirtyNum = 1;

  // Get move variables
  const Colour side = sideToMove;
  const Colour enemy = ~side;
  const Square from = move.from();
  const Square to = move.to();
  const Piece piece = getPiece(from);
  const Piece cap = move.isEnPassant() ? toPiece(enemy, PAWN) : getPiece(to);

  // Handle castling
  if (move.isCastle()) {
    // Move rook
    Square rookFrom, rookTo;
    Piece rook = toPiece(side, ROOK);
    castleRook<true>(from, to, rookFrom, rookTo);
    auto &dp = st->nnueData.dirtyPiece;
    dp.pc[1] = toNNUEPiece(toPiece(side, ROOK));
    dp.from[1] = rookFrom;
    dp.to[1] = rookTo;
    dp.dirtyNum = 2;
    // Update hash key
    hashKey ^= Zobrist::pieceSquareKeys[rook][rookFrom] ^
               Zobrist::pieceSquareKeys[rook][rookTo];
  }

  // Handle captures
  if (cap != NO_PIECE) {
    Square capSq = to;
    // Handle enpassant
    if (move.isEnPassant()) {
      // Change captured square to the enpassant square
      capSq += (sideToMove == WHITE ? S : N);
    }
    // Update board by removing piece on the destination
    popPiece(capSq);
    occupiedBB[enemy] &= ~squareBB(capSq);
    // Update hash key
    hashKey ^= Zobrist::pieceSquareKeys[cap][capSq];

    if (toPieceType(cap) != PAWN)
      st->nonPawnMaterial[enemy] -= PieceValue[cap];

    dp.dirtyNum = 2; // 1 piece moved, 1 piece captured
    dp.pc[1] = toNNUEPiece(cap);
    dp.from[1] = capSq;
    dp.to[1] = NO_SQ;

    // Update board state to undo move
    st->captured = cap;
  } else
    st->captured = NO_PIECE;

  // Update enpassant square
  if (st->enPassant != NO_SQ) {
    // Update hash key
    hashKey ^= Zobrist::enPassantKeys[fileOf(st->enPassant)];
    // Remove enpassant square
    st->enPassant = NO_SQ;
  }

  // Enpassant Square update
  if (toPieceType(piece) == PAWN && std::abs(rankOf(to) - rankOf(from)) == 2) {
    st->enPassant = from + ((sideToMove == WHITE) ? N : S);
    // Update hash key
    hashKey ^= Zobrist::enPassantKeys[fileOf(st->enPassant)];
  }

  dp.pc[0] = toNNUEPiece(piece);
  dp.from[0] = from;
  dp.to[0] = to;

  // Update board by moving piece to the destination
  movePiece(from, to);
  // Update hash key
  hashKey ^= Zobrist::pieceSquareKeys[piece][from] ^
             Zobrist::pieceSquareKeys[piece][to];

  if (toPieceType(piece) == PAWN) {
    // Handle promotions
    if (move.isPromotion()) {
      Piece promotedTo = toPiece(side, move.promoted());
      // Swap the piece on the promotion square
      popPiece(to);
      putPiece(promotedTo, to);

      // Update check mask for promotions
      if (toPieceType(promotedTo) == KNIGHT &&
          attacksBB<KNIGHT>(to, EMPTYBB) & getPiecesBB(enemy, KING))
        st->checkMask = squareBB(to);

      dp.to[0] = NO_SQ;
      dp.pc[dp.dirtyNum] = toNNUEPiece(promotedTo);
      dp.from[dp.dirtyNum] = NO_SQ;
      dp.to[dp.dirtyNum] = to;
      dp.dirtyNum++;

      // Update hash key
      hashKey ^= Zobrist::pieceSquareKeys[piece][to] ^
                 Zobrist::pieceSquareKeys[promotedTo][to];

      st->nonPawnMaterial[side] += PieceValue[promotedTo];
    }
    // Update fifty move rule
    st->fiftyMove = 0;

    // Handle pawn checks to update check mask
    Bitboard pawnAttack =
        (side == WHITE) ? pawnAttacksBB<WHITE>(to) : pawnAttacksBB<BLACK>(to);
    if (pawnAttack & getPiecesBB(enemy, KING))
      st->checkMask = squareBB(to);
  }

  // Update checkmask for piece moves
  if (toPieceType(piece) == KNIGHT &&
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
  if (move.isPromotion()) {
    popPiece(to);
    putPiece(toPiece(side, PAWN), to);
  }

  // Restore castling
  if (move.isCastle()) {
    Square rookFrom, rookTo;
    castleRook<false>(from, to, rookFrom, rookTo);
  }

  // Restore move
  movePiece(to, from);

  // Restore captures
  if (st->captured != NO_PIECE) {
    Square capSq = to;
    // Restore enpassant
    if (move.isEnPassant()) {
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
      getPiece(from) == NO_PIECE || toColour(getPiece(from)) != us ||
      getPieceType(to) == KING ||
      (toColour(getPiece(to)) == us and getPiece(to) != NO_PIECE)) {
    return false;
  }

  if (move.isPromotion() and move.promoted() == NO_PIECE_TYPE)
    return false;

  if (move.isEnPassant() and (st->enPassantPin || st->enPassant != to)) {
    return false;
  }

  if (move.isCastle()) {
    Bitboard castlingSquares = us == WHITE ? (to > from ? F1 | G1 : D1 | C1)
                                           : (to > from ? F8 | G8 : D8 | C8);
    Square rookSquare =
        to > from ? relativeSquare(us, H1) : relativeSquare(us, A1);

    if (((getOccupiedBB() | st->kingBan) & castlingSquares) ||
        square<KING>(us) != from || getPiece(rookSquare) != toPiece(us, ROOK) ||
        isInCheck()) {
      return false;
    }
  }

  if (move.isNormal()) {
    bool isCapture, isSinglePush, isDoublePush;
    Bitboard attacks;
    switch (getPieceType(from)) {
    case PAWN:
      attacks = (us == WHITE) ? pawnAttacksBB<WHITE>(from)
                              : pawnAttacksBB<BLACK>(from);
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
      if ((st->kingBan & to) || (squareDist(from, to) > 1 and !move.isCastle()))
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
  } else if (getPieceType(from) != PAWN and getPieceType(from) != KING)
    return false;

  if (getPieceType(from) != KING and isInCheck() and
      !(st->checkMask & (move.isEnPassant() ? to - pawnPush(us) : to)))
    return false;

  Bitboard pins = st->bishopPin | st->rookPin;

  return !(pins & from) || aligned(from, to, square<KING>(us));
}

Bitboard Position::pinners() const {
  return st->bishopPin & getPiecesBB(~sideToMove, BISHOP, QUEEN) |
         st->rookPin & getPiecesBB(~sideToMove, ROOK, QUEEN);
}

Bitboard Position::blockersForKing() const {
  return st->bishopPin & getOccupiedBB(sideToMove) |
         st->rookPin & getOccupiedBB(sideToMove);
}

// Stockfish SEE function
bool Position::SEE(Move move, int threshold) const {
  if (!move.isNormal())
    return 0 >= threshold;

  Square from = move.from();
  Square to = move.to();

  int swap = PieceValue[getPiece(to)] - threshold;

  // If capturing enemy piece does not go beyond the threshold, the give up
  if (swap < 0)
    return false;

  swap = PieceValue[getPiece(from)] - swap;
  if (swap <= 0)
    return true;

  Bitboard occupied = getOccupiedBB() ^ from ^ to;
  Colour stm = sideToMove;
  Bitboard attackers = sqAttackedByBB(to, occupied);
  Bitboard stmAttackers, bb;
  int res = 1;

  while (true) {
    stm = ~stm;

    attackers &= occupied; // Remove pieces that are already captured

    if (!(stmAttackers = attackers & getOccupiedBB(stm)))
      break;

    if (pinners() & occupied) {
      stmAttackers &= ~blockersForKing();

      if (!stmAttackers)
        break;
    }

    res ^= 1;

    // Locate and remove the next least valuable attacker, and add to
    // the bitboard 'attackers' any X-ray attackers behind it.
    if ((bb = stmAttackers & getPiecesBB(PAWN))) {
      if ((swap = PawnValue - swap) < res)
        break;
      occupied ^= getLSB(bb);

      attackers |= attacksBB<BISHOP>(to, occupied) & getPiecesBB(BISHOP, QUEEN);
    }

    else if ((bb = stmAttackers & getPiecesBB(KNIGHT))) {
      if ((swap = KnightValue - swap) < res)
        break;
      occupied ^= getLSB(bb);
    }

    else if ((bb = stmAttackers & getPiecesBB(BISHOP))) {
      if ((swap = BishopValue - swap) < res)
        break;
      occupied ^= getLSB(bb);

      attackers |= attacksBB<BISHOP>(to, occupied) & getPiecesBB(BISHOP, QUEEN);
    }

    else if ((bb = stmAttackers & getPiecesBB(ROOK))) {
      if ((swap = RookValue - swap) < res)
        break;
      occupied ^= getLSB(bb);

      attackers |= attacksBB<ROOK>(to, occupied) & getPiecesBB(ROOK, QUEEN);
    }

    else if ((bb = stmAttackers & getPiecesBB(QUEEN))) {
      if ((swap = QueenValue - swap) < res)
        break;
      occupied ^= getLSB(bb);

      attackers |=
          (attacksBB<BISHOP>(to, occupied) & getPiecesBB(BISHOP, QUEEN)) |
          (attacksBB<ROOK>(to, occupied) & getPiecesBB(ROOK, QUEEN));
    }

    else // KING
         // If we "capture" with the king but the opponent still has attackers,
         // reverse the result.
      return (attackers & ~getOccupiedBB(stm)) ? res ^ 1 : res;
  }

  return bool(res);
}
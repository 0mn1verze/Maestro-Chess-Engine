#include <algorithm>
#include <cstring>
#include <iostream>
#include <sstream>

#include "bitboard.hpp"
#include "defs.hpp"
#include "eval.hpp"
#include "hash.hpp"
#include "movegen.hpp"
#include "nnue.hpp"
#include "position.hpp"
#include "thread.hpp"
#include "utils.hpp"

namespace Maestro {

/******************************************\
|==========================================|
|          State Copy Constructor          |
|==========================================|
\******************************************/

BoardState BoardState::moveCopy(const BoardState &bs) {
  enPassant = bs.enPassant;
  plies = bs.plies;
  fiftyMove = bs.fiftyMove;
  nonPawnMaterial[WHITE] = bs.nonPawnMaterial[WHITE];
  nonPawnMaterial[BLACK] = bs.nonPawnMaterial[BLACK];
  psq = bs.psq;
  gamePhase = bs.gamePhase;
  castling = bs.castling;
  checkMask = FULLBB;
  kingBan = EMPTYBB;
  return *this;
}

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
  // Update piece count list
  pieceCount[toPiece(colourOf(piece), ALL_PIECES)]++;
  // Update piece list
  index[sq] = pieceCount[piece]++;
  pieceList[piece][index[sq]] = sq;
  // Update game phase
  st->gamePhase += Eval::gamePhaseInc[pieceTypeOf(piece)];
  // Update psq score
  st->psq += Eval::psqt[piece][sq];
}

// Remove piece on square
void Position::popPiece(Square sq) {
  // Find piece on square
  Piece pc = board[sq];
  // Update piece bitboards
  piecesBB[ALL_PIECES] ^= sq;
  piecesBB[pieceTypeOf(pc)] ^= sq;
  // Update occupancy bitboards
  occupiedBB[colourOf(pc)] ^= sq;
  // Update piece list at square
  board[sq] = NO_PIECE;
  pieceCount[toPiece(colourOf(pc), ALL_PIECES)]--;
  // Update piece count
  Square lastSq = pieceList[pc][--pieceCount[pc]];
  index[lastSq] = index[sq];
  pieceList[pc][index[lastSq]] = lastSq;
  pieceList[pc][pieceCount[pc]] = NO_SQ;
  // Update game phase
  st->gamePhase -= Eval::gamePhaseInc[pieceTypeOf(pc)];
  // Update psq score
  st->psq -= Eval::psqt[pc][sq];
}

// Move piece between two squares without updating piece counts (Quicker)
void Position::movePiece(Square from, Square to) {
  // Find piece on square
  Piece pc = board[from];
  // Update piece bitboards
  piecesBB[ALL_PIECES] ^= from | to;
  piecesBB[pieceTypeOf(pc)] ^= from | to;
  // Update occupancy bitboards
  occupiedBB[colourOf(pc)] ^= from | to;
  // Update piece list
  board[from] = NO_PIECE;
  board[to] = pc;
  // Update piece index
  index[to] = index[from];
  pieceList[pc][index[to]] = to;
  // Update psq score
  st->psq += Eval::psqt[pc][to] - Eval::psqt[pc][from];
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

  // Print eval
  std::cout << "Eval: " << Eval::evaluate(*this) << std::endl;

  // Print game phase
  std::cout << "Fen string: " << fen() << std::endl;
}

// Set the position based on fen string
void Position::set(const std::string &fen, BoardState &state, Thread *th) {
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
  std::fill_n(&pieceList[0][0], 16 * PIECE_N, NO_SQ);

  // Reset state
  state = {};

  // Set board state
  st = &state;

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
  is >> std::skipws >> st->fiftyMove >> pliesFromStart;

  pliesFromStart =
      std::max(2 * (pliesFromStart - 1), 0) + (sideToMove == BLACK);

  setState();

  // Set thread
  thisThread = th;
}

void Position::setState() const {

  st->key = initKey();

  st->pawnKey = initPawnKey();

  auto [whiteNPM, blackNPM] = initNonPawnMaterial();

  st->nonPawnMaterial[WHITE] = whiteNPM;
  st->nonPawnMaterial[BLACK] = blackNPM;

  st->psq = initPSQT();

  st->gamePhase = initGamePhase();

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

  if (canCastle(WK_SIDE))
    os << 'K';

  if (canCastle(WQ_SIDE))
    os << 'Q';

  if (canCastle(BK_SIDE))
    os << 'k';

  if (canCastle(BQ_SIDE))
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
|             Key Calculations             |
|==========================================|
\******************************************/

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

std::pair<int, int> Position::initNonPawnMaterial() const {
  // Initialize non pawn material
  int whiteNPM = 0, blackNPM = 0;
  // Loop through all pieces
  for (PieceType pt = KNIGHT; pt <= KING; ++pt) {
    // Update non pawn material
    whiteNPM +=
        Eval::pieceScore[toPiece(WHITE, pt)] * pieceCount[toPiece(WHITE, pt)];
    blackNPM +=
        Eval::pieceScore[toPiece(BLACK, pt)] * pieceCount[toPiece(BLACK, pt)];
  }

  return {whiteNPM, blackNPM};
}

Score Position::initPSQT() const {
  // Initialise piece square value
  Score psq = SCORE_ZERO;
  // Loop through all squares
  for (Square square = A1; square <= H8; ++square) {
    // Get piece on the square
    Piece piece = getPiece(square);
    // If there is a piece on the square, update piece square value
    if (piece != NO_PIECE) {
      psq += Eval::psqt[piece][square];
    }
  }

  return psq;
}

int Position::initGamePhase() const {
  // Initialize game phase
  int gamePhase = 0;
  // Loop through all pieces
  for (PieceType pt = KNIGHT; pt <= KING; ++pt) {
    // Update game phase
    gamePhase += Eval::gamePhaseInc[pt] * pieceCount[toPiece(WHITE, pt)];
    gamePhase += Eval::gamePhaseInc[pt] * pieceCount[toPiece(BLACK, pt)];
  }
  return gamePhase;
}

/******************************************\
|==========================================|
|             Board functions              |
|==========================================|
\******************************************/

// Check if the position is draw
bool Position::isDraw(int ply) const {
  if (st->fiftyMove > 99 and (!isInCheck() || MoveList<ALL>(*this).size()))
    return true;

  return st->repetition && st->repetition < ply;
}

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

/******************************************\
|==========================================|
|               Make/Unmake                |
|==========================================|
\******************************************/

void Position::makeMove(Move move, BoardState &state) {
  // Reset new state
  state = {};
  state.moveCopy(*st);
  // std::memcpy(&state, st, sizeof(BoardState));

  // Get Hash Key (And change sides)
  Key hashKey = st->key ^ Zobrist::sideKey;
  Key pawnKey = st->pawnKey;

  state.previous = st;
  st = &state;

  st->nnueData.accumulator.computedAccumulation = false;
  auto &dp = st->nnueData.dirtyPiece;
  dp.dirtyNum = 1;

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

    auto &dp = st->nnueData.dirtyPiece;
    dp.pc[1] = Eval::toNNUEPiece(toPiece(side, ROOK));
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
    if (move.is<EN_PASSANT>()) {
      // Change captured square to the enpassant square
      capSq += (sideToMove == WHITE ? S : N);
    }
    // Update board by removing piece on the destination
    popPiece(capSq);
    occupiedBB[enemy] &= ~squareBB(capSq);
    // Update hash key
    hashKey ^= Zobrist::pieceSquareKeys[cap][capSq];

    if (pieceTypeOf(cap) != PAWN)
      st->nonPawnMaterial[enemy] -= Eval::pieceScore[cap];
    else
      pawnKey ^= Zobrist::pieceSquareKeys[cap][capSq];

    dp.dirtyNum = 2; // 1 piece moved, 1 piece captured
    dp.pc[1] = Eval::toNNUEPiece(cap);
    dp.from[1] = capSq;
    dp.to[1] = NO_SQ;

    st->fiftyMove = 0;

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

  dp.pc[0] = Eval::toNNUEPiece(piece);
  dp.from[0] = from;
  dp.to[0] = to;

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

      dp.to[0] = NO_SQ;
      dp.pc[dp.dirtyNum] = Eval::toNNUEPiece(promotedTo);
      dp.from[dp.dirtyNum] = NO_SQ;
      dp.to[dp.dirtyNum] = to;
      dp.dirtyNum++;

      // Update hash key
      hashKey ^= Zobrist::pieceSquareKeys[piece][to] ^
                 Zobrist::pieceSquareKeys[promotedTo][to];

      pawnKey ^= Zobrist::pieceSquareKeys[piece][to];

      st->nonPawnMaterial[side] += Eval::pieceScore[promotedTo];
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

void Position::unmakeMove(Move move) {
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
  state.moveCopy(*st);

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
  Piece piece = getPiece(from);

  if (!isPseudoLegal(move))
    return false;

  // Handle enpassant
  if (move.is<EN_PASSANT>()) {
    Square ksq = square<KING>(us);
    Square capSq = to - pawnPush(us);
    Bitboard occ = getOccupiedBB() ^ from ^ capSq | to;

    return !(attacksBB<ROOK>(ksq, occ) & getPiecesBB(~us, ROOK, QUEEN)) &&
           !(attacksBB<BISHOP>(ksq, occ) & getPiecesBB(~us, BISHOP, QUEEN));
  }

  // Handle castling
  if (move.is<CASTLE>()) {
    to = relativeSquare(us, to > from ? G1 : C1);
    Bitboard b = betweenBB[from][to] | to;

    if (attackedByBB(~us) & b)
      return false;

    return true;
  }

  if (pieceTypeOf(piece) == KING)
    return !(st->kingBan & to);

  return !(st->pinned[us] & from) || aligned(from, to, square<KING>(us));
}

bool Position::isPseudoLegal(Move move) const {
  Colour us = sideToMove;
  Square from = move.from();
  Square to = move.to();
  Piece piece = getPiece(from);

  // If from square is not occupied by a piece belonging to the side to move
  if (piece == NO_PIECE || colourOf(piece) != us)
    return false;

  // If destination cannot be occupied by the piece
  if (getOccupiedBB(us) & to)
    return false;

  if (!move.is<NORMAL>())
    return true;

  // Handle pawn moves
  if (pieceTypeOf(piece) == PAWN) {
    bool isCapture = pawnAttacksBB(us, from) & getOccupiedBB(~us) & to;
    bool isSinglePush = (from + pawnPush(us) == to) && !(getOccupiedBB() & to);
    bool isDoublePush = (from + 2 * pawnPush(us) == to) &&
                        (rankOf(from) == relativeRank(us, RANK_2)) &&
                        !(getOccupiedBB() & to) &&
                        !(getOccupiedBB() & (to - pawnPush(us)));

    if (move.is<PROMOTION>() and move.promoted() == NO_PIECE_TYPE)
      return false;

    if (move.is<EN_PASSANT>() && !(pawnAttacksBB(us, from) & st->enPassant))
      return false;

    if (move.is<NORMAL>() && !isCapture && !isSinglePush && !isDoublePush)
      return false;
  }
  // If destination square is unreachable by the piece
  else if (!(attacksBB(pieceTypeOf(piece), from, getOccupiedBB()) & to))
    return false;

  if (isInCheck()) {
    if (pieceTypeOf(piece) != KING) {
      if (!(st->checkMask & to))
        return false;
    } else if (st->kingBan & to)
      return false;
  }

  return true;
}

// Stockfish SEE function
bool Position::SEE(Move move, int threshold) const {
  if (!move.is<NORMAL>())
    return 0 >= threshold;

  Square from = move.from();
  Square to = move.to();

  int swap = Eval::pieceScore[getPiece(to)] - threshold;

  // If capturing enemy piece does not go beyond the threshold, the give up
  if (swap < 0)
    return false;

  swap = Eval::pieceScore[getPiece(from)] - swap;
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
      if ((swap = Eval::pawnScore.first - swap) < res)
        break;
      occupied ^= getLSB(bb);

      attackers |= attacksBB<BISHOP>(to, occupied) & getPiecesBB(BISHOP, QUEEN);
    }

    else if ((bb = stmAttackers & getPiecesBB(KNIGHT))) {
      if ((swap = Eval::knightScore.first - swap) < res)
        break;
      occupied ^= getLSB(bb);
    }

    else if ((bb = stmAttackers & getPiecesBB(BISHOP))) {
      if ((swap = Eval::bishopScore.first - swap) < res)
        break;
      occupied ^= getLSB(bb);

      attackers |= attacksBB<BISHOP>(to, occupied) & getPiecesBB(BISHOP, QUEEN);
    }

    else if ((bb = stmAttackers & getPiecesBB(ROOK))) {
      if ((swap = Eval::rookScore.first - swap) < res)
        break;
      occupied ^= getLSB(bb);

      attackers |= attacksBB<ROOK>(to, occupied) & getPiecesBB(ROOK, QUEEN);
    }

    else if ((bb = stmAttackers & getPiecesBB(QUEEN))) {
      if ((swap = Eval::queenScore.first - swap) < res)
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

// Check if move gives check
bool Position::givesCheck(Move move) const {
  Square from = move.from();
  Square to = move.to();
  PieceType pt = getPieceType(from);
  Colour us = sideToMove;
  Square enemyKing = square<KING>(~us);

  if (attacksBB(pt, to, getOccupiedBB()) & enemyKing)
    return true;

  Bitboard attackers =
      attacksBB<BISHOP>(enemyKing, getPiecesBB(us, BISHOP, QUEEN)) |
      attacksBB<ROOK>(enemyKing, getPiecesBB(us, ROOK, QUEEN));

  attackers &= lineBB[enemyKing][from];

  if (attackers) {
    Square sq = popLSB(attackers);
    Bitboard b = betweenBB[sq][enemyKing] & getOccupiedBB();
    if (b and !moreThanOne(b))
      return !aligned(from, to, enemyKing) || move.is<CASTLE>();
  }

  Square capsq, rto;
  Bitboard b;

  switch (move.flag()) {
  case NORMAL:
    return false;
  case PROMOTION:
    return attacksBB(move.promoted(), to, getOccupiedBB() ^ from) & enemyKing;
  case EN_PASSANT:
    capsq = getEnPassantTarget(us);
    b = (getOccupiedBB() ^ from ^ capsq) | to;
    return (attacksBB<BISHOP>(enemyKing, b) & getPiecesBB(us, BISHOP, QUEEN)) |
           (attacksBB<ROOK>(enemyKing, b) & getPiecesBB(us, ROOK, QUEEN));
  case CASTLE:
    rto = relativeSquare(us, to > from ? F1 : D1);
    return attacksBB<ROOK>(rto, getOccupiedBB()) & enemyKing;
  default:
    return false;
  }

  return false;
}

} // namespace Maestro
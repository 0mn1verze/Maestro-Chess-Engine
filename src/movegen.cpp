#include "movegen.hpp"
#include "defs.hpp"
#include "move.hpp"
#include "position.hpp"
#include "utils.hpp"
#include <iostream>

namespace Maestro {

/******************************************\
|==========================================|
|              Move Gen Init               |
|==========================================|
\******************************************/

template <PieceType pt> void checkBySlider(const Position &pos, Square king) {
  const Colour us = pos.sideToMove();
  const Colour them = ~pos.sideToMove();
  BoardState *st = pos.state();
  Square sq;
  Bitboard enemyPieces = pos.pieces(them, pt, QUEEN);

  // Blockers (could be pieces on both sides that could be blocking an attack to
  // the king, or pieces attacking the king)
  Bitboard blockers = attacksBB<pt>(king, pos.occupied()) & pos.occupied();
  // If there are no blockers, there will not be any attackers or pins
  if (!blockers)
    return;
  // Pieces that directly attack the king
  Bitboard attackers = blockers & enemyPieces;

  while (attackers) {
    sq = popLSB(attackers);
    Bitboard pinMask = pinBB[king][sq];
    // Update check mask
    if (st->checkMask == FULLBB)
      st->checkMask = pinMask;
    // Double check
    else
      st->checkMask = EMPTYBB;
    // If there are checkers, remove the squares where the pieces attack.
    st->kingBan |= checkBB[king][sq];
  }

  // Pieces that pins pieces to the king
  Bitboard pinners =
      attacksBB<pt>(king, pos.occupied() ^ (blockers & ~enemyPieces)) &
      enemyPieces;

  // If there are no pins, there is not reason to update;
  if (!pinners)
    return;

  // If there are pinners, update the pinners and pinned bitboards
  if (pinners) {
    st->pinners[them] = pinners;
    // For each pinner, update the pin mask
    while (pinners) {
      sq = popLSB(pinners);
      Bitboard pinMask = pinBB[king][sq];

      // Update pinned bitboard
      st->pinned[us] |= pinMask & pos.occupied(us);

      // Update en passant pin
      if constexpr (pt == BISHOP)
        if (pinMask & pos.enPassantTarget(them))
          st->enPassantPin = true;

      // Update bishop and rook pin bitboard
      if constexpr (pt == BISHOP)
        st->bishopPin |= pinMask;
      else
        st->rookPin |= pinMask;
    }
  }
}

// Refresh masks
void refreshMasks(const Position &pos) {
  Square sq = NO_SQ;
  const Colour us = pos.sideToMove();
  const Colour them = ~us;
  const Square kingSquare = pos.square<KING>(us);
  BoardState *st = pos.state();

  // Define king attacks bitboard
  st->kingAttacks = attacksBB<KING>(kingSquare, EMPTYBB);

  // Reset pin masks
  st->rookPin = EMPTYBB;
  st->bishopPin = EMPTYBB;

  // Reset pinned pieces and pinners
  st->pinned[us] = EMPTYBB;
  st->pinners[them] = EMPTYBB;

  // Update slider attacks and pins
  checkBySlider<BISHOP>(pos, kingSquare);
  checkBySlider<ROOK>(pos, kingSquare);

  // Update enpassant pin
  if (st->enPassant != NO_SQ)
    refreshEPPin(pos);

  // Squares that the king attacks (excluding squares occupied by friendly
  // pieces and the squares that the king is "banned" from)
  st->kingAttacks &= ~(pos.occupied(us) | st->kingBan);

  // Squares that are available for friendly pieces
  st->available = st->checkMask & ~pos.occupied(us);

  // If the king does not attack squares (Trapped king), no need to check for
  // attacked squares
  if (st->kingAttacks == EMPTYBB)
    return;

  // Squares that are attacked by enemy pieces
  st->attacked = pos.attackedByBB(them);

  // Exclude squares that are attacked by enemy pieces from king attacks
  st->kingAttacks &= ~st->attacked;

  // Include squares that are attacked by enemy pieces in king ban
  st->kingBan |= st->attacked;
}

// Refresh en passant pin
void refreshEPPin(const Position &pos) {
  const Colour us = pos.sideToMove();
  const Colour them = ~us;
  const Bitboard king = pos.pieces(us, KING);
  const Bitboard pawns = pos.pieces(us, PAWN);
  const Bitboard enemyRQ = pos.pieces(them, ROOK, QUEEN);
  const Bitboard enPassantTarget = squareBB(pos.enPassantTarget(them));

  // const Bitboard EPRank = (them == WHITE) ? rankBB(RANK_4) : rankBB(RANK_5);
  const Bitboard EPRank = rankBB((them == WHITE) ? RANK_4 : RANK_5);

  if ((EPRank & king) && (EPRank & enemyRQ) && (EPRank & pawns) &&
      pos.enPassantTarget(them) != NO_SQ) {
    Bitboard pawnEPL = pawns & shift<E>(enPassantTarget);
    Bitboard pawnEPR = pawns & shift<W>(enPassantTarget);

    if (pawnEPL) {
      Bitboard afterCap = pos.occupied() & ~(enPassantTarget | pawnEPL);
      if (attacksBB<ROOK>(getLSB(king), afterCap) & EPRank & enemyRQ) {
        pos.state()->enPassantPin = true;
      }
    }

    if (pawnEPR) {
      Bitboard afterCap = pos.occupied() & ~(enPassantTarget | pawnEPR);
      if (attacksBB<ROOK>(getLSB(king), afterCap) & EPRank & enemyRQ) {
        pos.state()->enPassantPin = true;
      }
    }
  }
}

/******************************************\
|==========================================|
|     Move Generation Helper Functions     |
|==========================================|
\******************************************/

// Add pawn promotions
inline Move *addPawnPromotions(Move *moves, Bitboard bb, Direction dir) {
  while (bb) {
    const Square origin = popLSB(bb);
    for (PieceType pt : {KNIGHT, BISHOP, ROOK, QUEEN})
      *moves++ = Move::encode<PROMOTION>(origin, origin + dir, pt);
  }
  return moves;
}

// Add pawn moves
inline Move *addPawnMoves(Move *moves, Bitboard bb, Direction dir) {
  while (bb) {
    const Square origin = popLSB(bb);
    *moves++ = Move::encode(origin, origin + dir);
  }
  return moves;
}

// Add piece moves
inline Move *addPieceMoves(Move *moves, Bitboard bb, Square origin) {
  while (bb) {
    const Square destination = popLSB(bb);
    *moves++ = Move::encode(origin, destination);
  }
  return moves;
}

// Add piece moves with mask
template <PieceType pt, GenType gt>
inline Move *addPieceMoves(Move *moves, const Position &pos, Bitboard bb,
                           Bitboard masks, Colour them) {
  while (bb) {
    // Get origin square and pop it from the bitboard
    Square origin = popLSB(bb);
    // Generate queen non captures
    Bitboard attacks = attacksBB<pt>(origin, pos.occupied()) & masks;

    if constexpr (gt == CAPTURES)
      attacks &= pos.occupied(them);
    if constexpr (gt == QUIETS)
      attacks &= ~pos.occupied();

    // Add moves to move list
    moves = addPieceMoves(moves, attacks, origin);
  }
  return moves;
}

/******************************************\
|==========================================|
|              Move Generation             |
|==========================================|
\******************************************/

// Generate pawn moves
template <GenType gt, Colour us>
inline Move *generatePawnMoves(Move *moves, const Position &pos) {
  // Constants for the function (us = side to move, them = enemy)
  constexpr Colour them = ~us;
  constexpr Direction left = (us == WHITE) ? NW : SE;
  constexpr Direction right = (us == WHITE) ? NE : SW;
  constexpr Direction forward = (us == WHITE) ? N : S;
  constexpr Direction push = (us == WHITE) ? NN : SS;
  constexpr Direction EPLeft = (us == WHITE) ? E : W;
  constexpr Direction EPRight = -EPLeft;

  BoardState *st = pos.state();
  const bool noCheck = (st->checkMask == FULLBB);

  // Get masks from state
  const Bitboard bishopPin = st->bishopPin;
  const Bitboard rookPin = st->rookPin;
  const Bitboard checkMask = st->checkMask;

  // Promotion and push ranks
  constexpr Bitboard promotionRank = rankBB((us == WHITE) ? RANK_7 : RANK_2);
  constexpr Bitboard pushRank = rankBB((us == WHITE) ? RANK_2 : RANK_7);

  if constexpr (gt == CAPTURES || gt == ALL) {
    // Enemy Pieces
    const Bitboard enemyPieces = pos.occupied(them);
    // Pawns that can attack left and right
    const Bitboard pawnsLR = pos.pieces(us, PAWN) & ~rookPin;
    // Pawns that can attack left
    Bitboard pawnL = pawnsLR & shift<-left>(enemyPieces & checkMask) &
                     (shift<-left>(bishopPin) | ~bishopPin);
    // Pawns that can attack right
    Bitboard pawnR = pawnsLR & shift<-right>(enemyPieces & checkMask) &
                     (shift<-right>(bishopPin) | ~bishopPin);

    if (st->enPassant != NO_SQ && !st->enPassantPin) {
      const Bitboard enPassantTarget = squareBB(pos.enPassantTarget(them));
      // Left capture enpassant pawn
      Bitboard pawnEPL = pawnsLR & shift<EPLeft>(checkMask & enPassantTarget) &
                         (shift<-left>(bishopPin) | ~bishopPin);
      // Right capture enpassant pawn
      Bitboard pawnEPR = pawnsLR & shift<EPRight>(checkMask & enPassantTarget) &
                         (shift<-right>(bishopPin) | ~bishopPin);

      if (pawnEPL)
        // Add en passant move
        *moves++ =
            Move::encode<EN_PASSANT>(getLSB(pawnEPL), getLSB(pawnEPL) + left);
      if (pawnEPR)
        // Add en passant move
        *moves++ =
            Move::encode<EN_PASSANT>(getLSB(pawnEPR), getLSB(pawnEPR) + right);
    }

    if ((pawnL | pawnR) & promotionRank) {
      // Add left capture promotions
      moves = addPawnPromotions(moves, pawnL & promotionRank, left);
      // Add right capture promotions
      moves = addPawnPromotions(moves, pawnR & promotionRank, right);
      // Add left captures (non promotion)
      moves = addPawnMoves(moves, pawnL & ~promotionRank, left);
      // Add right captures (non promotion)
      moves = addPawnMoves(moves, pawnR & ~promotionRank, right);
    } else {
      // Add left captures
      moves = addPawnMoves(moves, pawnL, left);
      // Add right captures
      moves = addPawnMoves(moves, pawnR, right);
    }
  }

  if constexpr (gt == QUIETS || gt == ALL) {
    // Pawns that can move forward (pseudo legal)
    const Bitboard pawnFwd = pos.pieces(us, PAWN) & ~bishopPin;

    // Pawns that can move forward (pseudo legal, rook pin checked later)
    Bitboard pawnF = pawnFwd & shift<-forward>(~pos.occupied());
    // Pawns that can move forward two squares
    Bitboard pawnP = pawnF & shift<-push>(~pos.occupied() & checkMask) &
                     pushRank & (shift<-push>(rookPin) | ~rookPin);
    // Pawns that can move forward
    pawnF &= shift<-forward>(checkMask) & (shift<-forward>(rookPin) | ~rookPin);

    if (pawnF & promotionRank) {
      // Add forward promotions
      moves = addPawnPromotions(moves, pawnF & promotionRank, forward);
      // Add forward moves (non promotion)
      moves = addPawnMoves(moves, pawnF & ~promotionRank, forward);
      // Add push moves
      moves = addPawnMoves(moves, pawnP, push);
    } else {
      // Add forward moves
      moves = addPawnMoves(moves, pawnF, forward);
      // Add push moves
      moves = addPawnMoves(moves, pawnP, push);
    }
  }

  return moves;
}

// Generate pawn moves
template <GenType gt>
inline Move *generateKnightMoves(Move *moves, const Position &pos) {
  // Side to move and enemy
  const Colour us = pos.sideToMove();
  const Colour them = ~us;
  BoardState *st = pos.state();
  // masks
  Bitboard available = st->available;
  Bitboard rookPin = st->rookPin;
  Bitboard bishopPin = st->bishopPin;
  // Knights Bitboard (Pinned knights cannot move)
  Bitboard knights = pos.pieces(us, KNIGHT) & ~(rookPin | bishopPin);

  moves = addPieceMoves<KNIGHT, gt>(moves, pos, knights, available, them);

  return moves;
}

// Generate pawn moves
template <GenType gt>
inline Move *generateBishopMoves(Move *moves, const Position &pos) {
  // Side to move and enemy
  const Colour us = pos.sideToMove();
  const Colour them = ~us;
  BoardState *st = pos.state();
  // masks
  Bitboard available = st->available;
  Bitboard rookPin = st->rookPin;
  Bitboard bishopPin = st->bishopPin;
  // Queens Bitboard
  Bitboard queens = pos.pieces(us, QUEEN);
  // Bishops Bitboard (Horizontal pinned bishops cannot move)
  Bitboard bishops = pos.pieces(us, BISHOP) & ~rookPin;
  // Pinned bishops
  Bitboard pinned = (bishops | queens) & bishopPin;
  // Non pinned bishops
  Bitboard nonPinned = bishops & ~bishopPin;

  // Add pinned bishop moves
  moves = addPieceMoves<BISHOP, gt>(moves, pos, pinned, available & bishopPin,
                                    them);
  // Add non pinned bishop moves
  moves = addPieceMoves<BISHOP, gt>(moves, pos, nonPinned, available, them);

  return moves;
}

template <GenType gt>
inline Move *generateRookMoves(Move *moves, const Position &pos) {
  // Side to move and enemy
  const Colour us = pos.sideToMove();
  const Colour them = ~us;
  BoardState *st = pos.state();
  // masks
  Bitboard available = st->available;
  Bitboard rookPin = st->rookPin;
  Bitboard bishopPin = st->bishopPin;
  // Queens Bitboard
  Bitboard queens = pos.pieces(us, QUEEN);
  // Rooks Bitboard (Diagonal pinned rooks cannot move)
  Bitboard rooks = pos.pieces(us, ROOK) & ~bishopPin;
  // Pinned rooks
  Bitboard pinned = (rooks | queens) & rookPin;
  // Non pinned rooks
  Bitboard nonPinned = rooks & ~rookPin;

  // Add pinned rook moves
  moves =
      addPieceMoves<ROOK, gt>(moves, pos, pinned, available & rookPin, them);
  // Add non pinned rook moves
  moves = addPieceMoves<ROOK, gt>(moves, pos, nonPinned, available, them);

  return moves;
}

template <GenType gt>
inline Move *generateQueenMoves(Move *moves, const Position &pos) {
  // Side to move and enemy
  const Colour us = pos.sideToMove();
  const Colour them = ~us;
  BoardState *st = pos.state();
  // masks
  Bitboard available = st->available;
  Bitboard rookPin = st->rookPin;
  Bitboard bishopPin = st->bishopPin;
  // Queens Bitboard (Non pinned)
  Bitboard queens = pos.pieces(us, QUEEN) & ~(bishopPin | rookPin);
  // Add piece moves
  moves = addPieceMoves<QUEEN, gt>(moves, pos, queens, available, them);

  return moves;
}

template <GenType gt, Colour us>
Move *generateKingMoves(Move *moves, const Position &pos) {
  // Side to move and enemy
  constexpr Colour them = ~us;
  BoardState *st = pos.state();
  // King attacks
  Bitboard kingAttacks = st->kingAttacks;
  Square origin = pos.square<KING>(us);
  bool noCheck = (st->checkMask == FULLBB);

  if constexpr (gt == CAPTURES)
    // Generate only attacks if in captures mode
    kingAttacks &= pos.occupied(them);
  if constexpr (gt == QUIETS)
    // Generate only non captures if in quiet mode
    kingAttacks &= ~pos.occupied();

  // add king moves
  while (kingAttacks) {
    const Square destination = popLSB(kingAttacks);
    *moves++ = Move::encode(origin, destination);
  }

  if constexpr (gt == CAPTURES)
    // Generate only attacks if in captures mode
    return moves;

  if (!noCheck)
    return moves;

  // Castling variables
  constexpr Bitboard kingSideSquares = (us == WHITE) ? (F1 | G1) : (F8 | G8);
  constexpr Square kingSideDest = (us == WHITE) ? G1 : G8;
  constexpr Bitboard queenSideSquares = (us == WHITE) ? (C1 | D1) : (C8 | D8);
  constexpr Square queenSideDest = (us == WHITE) ? C1 : C8;
  constexpr Bitboard queenSideOccupiedSquares =
      (us == WHITE) ? queenSideSquares | B1 : queenSideSquares | B8;

  // Castling rights
  constexpr Castling kingSide = (us == WHITE) ? WK_SIDE : BK_SIDE;
  constexpr Castling queenSide = (us == WHITE) ? WQ_SIDE : BQ_SIDE;

  if (st->castling & kingSide) {
    // Check if king is not in check and squares are not occupied
    if (noCheck && !(st->kingBan & kingSideSquares) &&
        !(pos.occupied() & kingSideSquares))
      *moves++ = Move::encode<CASTLE>(origin, kingSideDest);
  }

  if (st->castling & queenSide) {
    // Check if king is not in check and squares are not occupied
    if (noCheck && !(st->kingBan & queenSideSquares) &&
        !(pos.occupied() & queenSideOccupiedSquares))
      *moves++ = Move::encode<CASTLE>(origin, queenSideDest);
  }

  return moves;
}

template <GenType gt> Move *generateMoves(Move *moves, const Position &pos) {

  if (pos.sideToMove() == WHITE)
    moves = generateKingMoves<gt, WHITE>(moves, pos);
  else
    moves = generateKingMoves<gt, BLACK>(moves, pos);

  if (pos.state()->checkMask == EMPTYBB)
    return moves;

  // Generate moves for each piece
  if (pos.sideToMove() == WHITE)
    moves = generatePawnMoves<gt, WHITE>(moves, pos);
  else
    moves = generatePawnMoves<gt, BLACK>(moves, pos);

  moves = generateKnightMoves<gt>(moves, pos);
  moves = generateBishopMoves<gt>(moves, pos);
  moves = generateRookMoves<gt>(moves, pos);
  moves = generateQueenMoves<gt>(moves, pos);

  return moves;
}

// Explicit instantiation
template Move *generateMoves<ALL>(Move *moves, const Position &pos);
template Move *generateMoves<CAPTURES>(Move *moves, const Position &pos);
template Move *generateMoves<QUIETS>(Move *moves, const Position &pos);

} // namespace Maestro
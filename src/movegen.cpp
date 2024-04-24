#include "movegen.hpp"
#include "position.hpp"
#include "utils.hpp"
#include <iostream>

/******************************************\
|==========================================|
|              Move Gen Init               |
|==========================================|
\******************************************/

template <PieceType pt> void checkBySlider(const Position &pos, Square king) {
  const Colour them = ~pos.getSideToMove();
  BoardState *st = pos.state();
  Square sq;
  Bitboard enemyPieces = pos.getPiecesBB(them, pt, QUEEN);
  // Pieces that attack the king (Could be through enemy pieces)
  Bitboard pinners = attacksBB<pt>(king, pos.getOccupiedBB(them)) & enemyPieces;

  if (pinners) {
    Bitboard attacks = attacksBB<pt>(king, pos.getOccupiedBB()) & enemyPieces;
    pinners &= ~attacks;

    while (attacks) {
      sq = popLSB(attacks);
      // If there are no pieces in between the attacker and the king,
      // update the check mask
      if (st->checkMask == FULLBB)
        st->checkMask = pinBB[king][sq];
      else
        // Double check = king has to move
        st->checkMask = EMPTYBB;
      st->kingBan |= checkBB[king][sq];
    }

    while (pinners) {
      sq = popLSB(pinners);
      Bitboard pinMask = pinBB[king][sq];
      Bitboard pinned = pinMask & pos.getOccupiedBB(~them);

      // Handle enpassant pin
      if constexpr (pt == BISHOP)
        if (pinBB[king][sq] & pos.getEnPassantTarget(them))
          st->enPassantPin = true;

      if (pinned and !moreThanOne(pinned)) {
        if constexpr (pt == BISHOP)
          st->bishopPin |= pinBB[king][sq];
        else
          st->rookPin |= pinBB[king][sq];
      }
    }
  }
}

// Refresh masks
void refreshMasks(const Position &pos) {
  Square sq = NO_SQ;
  const Colour us = pos.getSideToMove();
  const Colour them = ~us;
  const Square kingSquare = pos.square<KING>(us);
  BoardState *st = pos.state();

  // Define pinners bitboard
  Bitboard pinners = EMPTYBB;

  // Define king attacks bitboard
  st->kingAttacks = attacksBB<KING>(kingSquare, EMPTYBB);

  // if (kingSquare == NO_SQ) {
  //   pos.print();
  //   BoardState *st = pos.state();
  //   while (st->previous != nullptr) {
  //     std::cout << move2Str(st->move) << std::endl;
  //     st = st->previous;
  //   }
  //   throw std::runtime_error("King not found");
  // }

  st->rookPin = EMPTYBB;
  st->bishopPin = EMPTYBB;

  checkBySlider<BISHOP>(pos, kingSquare);
  checkBySlider<ROOK>(pos, kingSquare);

  if (st->enPassant != NO_SQ)
    refreshEPPin(pos);

  st->kingAttacks &= ~(pos.getOccupiedBB(us) | st->kingBan);

  st->available = st->checkMask & ~pos.getOccupiedBB(us);

  if (st->kingAttacks == EMPTYBB)
    return;

  st->attacked = pos.attackedByBB(them);

  st->kingAttacks &= ~st->attacked;

  st->kingBan |= st->attacked;
}

// Refresh en passant pin
void refreshEPPin(const Position &pos) {
  const Colour us = pos.getSideToMove();
  const Colour them = ~us;
  const Bitboard king = pos.getPiecesBB(us, KING);
  const Bitboard pawns = pos.getPiecesBB(us, PAWN);
  const Bitboard enemyRQ = pos.getPiecesBB(them, ROOK, QUEEN);
  const Bitboard enPassantTarget = squareBB(pos.getEnPassantTarget(them));

  const Bitboard EPRank = (them == WHITE) ? rankBB(RANK_4) : rankBB(RANK_5);

  if ((EPRank & king) && (EPRank & enemyRQ) && (EPRank & pawns) &&
      pos.getEnPassantTarget(them) != NO_SQ) {
    Bitboard pawnEPL = pawns & shift<E>(enPassantTarget);
    Bitboard pawnEPR = pawns & shift<W>(enPassantTarget);

    if (pawnEPL) {
      Bitboard afterCap = pos.getOccupiedBB() & ~(enPassantTarget | pawnEPL);
      if (attacksBB<ROOK>(getLSB(king), afterCap) & EPRank & enemyRQ) {
        pos.state()->enPassantPin = true;
      }
    }

    if (pawnEPR) {
      Bitboard afterCap = pos.getOccupiedBB() & ~(enPassantTarget | pawnEPR);
      if (attacksBB<ROOK>(getLSB(king), afterCap) & EPRank & enemyRQ) {
        pos.state()->enPassantPin = true;
      }
    }
  }
}

// Add pawn moves
inline Move *addPawnPromotions(Move *moves, Square from, Square to) {
  for (PieceType pt : {KNIGHT, BISHOP, ROOK, QUEEN})
    *moves++ = Move::encode<PROMOTION>(from, to, pt);
  return moves;
}

// Generate pawn moves
template <GenType gt, Colour us>
Move *generatePawnMoves(Move *moves, const Position &pos) {
  constexpr Colour them = ~us;
  constexpr Direction left = (us == WHITE) ? NW : SE;
  constexpr Direction right = (us == WHITE) ? NE : SW;
  constexpr Direction forward = (us == WHITE) ? N : S;
  constexpr Direction push = (us == WHITE) ? NN : SS;
  constexpr Direction EPLeft = (us == WHITE) ? E : W;
  constexpr Direction EPRight = -EPLeft;
  BoardState *st = pos.state();
  const bool noCheck = (st->checkMask == FULLBB);

  const Bitboard bishopPin = st->bishopPin;
  const Bitboard rookPin = st->rookPin;
  const Bitboard checkMask = st->checkMask;

  constexpr Bitboard promotionRank =
      (us == WHITE) ? rankBB(RANK_7) : rankBB(RANK_2);
  constexpr Bitboard pushRank = (us == WHITE) ? rankBB(RANK_2) : rankBB(RANK_7);

  if constexpr (gt == CAPTURES || gt == ALL) {
    // Enemy Pieces
    const Bitboard enemyPieces = pos.getOccupiedBB(them);
    // Pawns that can attack left and right
    const Bitboard pawnsLR = pos.getPiecesBB(us, PAWN) & ~rookPin;
    // Pawns that can attack left
    Bitboard pawnL = pawnsLR & shift<-left>(enemyPieces & checkMask) &
                     (shift<-left>(bishopPin) | ~bishopPin);
    // Pawns that can attack right
    Bitboard pawnR = pawnsLR & shift<-right>(enemyPieces & checkMask) &
                     (shift<-right>(bishopPin) | ~bishopPin);

    if (st->enPassant != NO_SQ && !st->enPassantPin) {
      const Bitboard enPassantTarget = squareBB(pos.getEnPassantTarget(them));
      // Left capture enpassant pawn
      Square pawnEPL =
          getLSB(pawnsLR & shift<EPLeft>(checkMask & enPassantTarget) &
                 (shift<-left>(bishopPin) | ~bishopPin));
      Square pawnEPR =
          getLSB(pawnsLR & shift<EPRight>(checkMask & enPassantTarget) &
                 (shift<-right>(bishopPin) | ~bishopPin));

      if (pawnEPL != NO_SQ)
        *moves++ = Move::encode<EN_PASSANT>(pawnEPL, pawnEPL + left);
      if (pawnEPR != NO_SQ)
        *moves++ = Move::encode<EN_PASSANT>(pawnEPR, pawnEPR + right);
    }

    if ((pawnL | pawnR) & promotionRank) {
      Bitboard promoteL = pawnL & promotionRank;
      Bitboard promoteR = pawnR & promotionRank;
      Bitboard nonPromoteL = pawnL & ~promotionRank;
      Bitboard nonPromoteR = pawnR & ~promotionRank;

      while (promoteL) {
        const Square origin = popLSB(promoteL);
        moves = addPawnPromotions(moves, origin, origin + left);
      }
      while (promoteR) {
        const Square origin = popLSB(promoteR);
        moves = addPawnPromotions(moves, origin, origin + right);
      }
      while (nonPromoteL) {
        const Square origin = popLSB(nonPromoteL);
        *moves++ = Move::encode(origin, origin + left);
      }
      while (nonPromoteR) {
        const Square origin = popLSB(nonPromoteR);
        *moves++ = Move::encode(origin, origin + right);
      }
    } else {
      while (pawnL) {
        const Square origin = popLSB(pawnL);
        *moves++ = Move::encode(origin, origin + left);
      }
      while (pawnR) {
        const Square origin = popLSB(pawnR);
        *moves++ = Move::encode(origin, origin + right);
      }
    }
  }

  if constexpr (gt == QUIETS || gt == ALL) {
    const Bitboard pawnFwd = pos.getPiecesBB(us, PAWN) & ~bishopPin;

    Bitboard pawnF = pawnFwd & shift<-forward>(~pos.getOccupiedBB());
    Bitboard pawnP = pawnF & shift<-push>(~pos.getOccupiedBB() & checkMask) &
                     pushRank & (shift<-push>(rookPin) | ~rookPin);
    pawnF &= shift<-forward>(checkMask) & (shift<-forward>(rookPin) | ~rookPin);

    if (pawnF & promotionRank) {
      Bitboard promoteF = pawnF & promotionRank;
      Bitboard nonPromoteF = pawnF & ~promotionRank;

      while (promoteF) {
        const Square origin = popLSB(promoteF);
        moves = addPawnPromotions(moves, origin, origin + forward);
      }
      while (nonPromoteF) {
        const Square origin = popLSB(nonPromoteF);
        *moves++ = Move::encode(origin, origin + forward);
      }
      while (pawnP) {
        const Square origin = popLSB(pawnP);
        *moves++ = Move::encode(origin, origin + push);
      }
    } else {
      while (pawnF) {
        const Square origin = popLSB(pawnF);
        *moves++ = Move::encode(origin, origin + forward);
      }
      while (pawnP) {
        const Square origin = popLSB(pawnP);
        *moves++ = Move::encode(origin, origin + push);
      }
    }
  }

  return moves;
}

// Generate pawn moves
template <GenType gt>
Move *generateKnightMoves(Move *moves, const Position &pos) {
  // Side to move and enemy
  const Colour us = pos.getSideToMove();
  const Colour them = ~us;
  BoardState *st = pos.state();
  // masks
  Bitboard available = st->available;
  Bitboard rookPin = st->rookPin;
  Bitboard bishopPin = st->bishopPin;
  // Knights Bitboard (Pinned knights cannot move)
  Bitboard knights = pos.getPiecesBB(us, KNIGHT) & ~(rookPin | bishopPin);

  while (knights) {
    // Get origin square and pop it from the bitboard
    Square origin = popLSB(knights);
    // Generate knight attacks
    Bitboard attacks = attacksBB<KNIGHT>(origin, EMPTYBB) & available;

    if constexpr (gt == CAPTURES)
      // Generate only attacks if in captures mode
      attacks &= pos.getOccupiedBB(them);
    if constexpr (gt == QUIETS)
      // Generate only non captures if in quiet mode
      attacks &= ~pos.getOccupiedBB();

    // Add moves to move list
    while (attacks) {
      const Square destination = popLSB(attacks);
      *moves++ = Move::encode(origin, destination);
    }
  }

  return moves;
}

// Generate pawn moves
template <GenType gt>
Move *generateBishopMoves(Move *moves, const Position &pos) {
  // Side to move and enemy
  const Colour us = pos.getSideToMove();
  const Colour them = ~us;
  BoardState *st = pos.state();
  // masks
  Bitboard available = st->available;
  Bitboard rookPin = st->rookPin;
  Bitboard bishopPin = st->bishopPin;
  // Queens Bitboard
  Bitboard queens = pos.getPiecesBB(us, QUEEN);
  // Bishops Bitboard (Horizontal pinned bishops cannot move)
  Bitboard bishops = pos.getPiecesBB(us, BISHOP) & ~rookPin;
  // Pinned bishops
  Bitboard pinned = (bishops | queens) & bishopPin;
  // Non pinned bishops
  Bitboard nonPinned = bishops & ~bishopPin;

  while (pinned) {
    // Get origin square and pop it from the bitboard
    Square origin = popLSB(pinned);
    // Generate bishop attacks (Pinned bishops)
    Bitboard attacks =
        attacksBB<BISHOP>(origin, pos.getOccupiedBB()) & available & bishopPin;

    if constexpr (gt == CAPTURES)
      // Generate only attacks if in captures mode
      attacks &= pos.getOccupiedBB(them);
    if constexpr (gt == QUIETS)
      // Generate only non captures if in quiet mode
      attacks &= ~pos.getOccupiedBB();

    // Add moves to move list
    while (attacks) {
      const Square destination = popLSB(attacks);
      *moves++ = Move::encode(origin, destination);
    }
  }

  while (nonPinned) {
    // Get origin square and pop it from the bitboard
    Square origin = popLSB(nonPinned);
    // Generate bishop attacks (Non pinned bishops)
    Bitboard attacks =
        attacksBB<BISHOP>(origin, pos.getOccupiedBB()) & available;

    if constexpr (gt == CAPTURES)
      // Generate only attacks if in captures mode
      attacks &= pos.getOccupiedBB(them);
    if constexpr (gt == QUIETS)
      // Generate only non captures if in quiet mode
      attacks &= ~pos.getOccupiedBB();

    // Add moves to move list
    while (attacks) {
      const Square destination = popLSB(attacks);
      *moves++ = Move::encode(origin, destination);
    }
  }

  return moves;
}

template <GenType gt>
Move *generateRookMoves(Move *moves, const Position &pos) {
  // Side to move and enemy
  const Colour us = pos.getSideToMove();
  const Colour them = ~us;
  BoardState *st = pos.state();
  // masks
  Bitboard available = st->available;
  Bitboard rookPin = st->rookPin;
  Bitboard bishopPin = st->bishopPin;
  // Queens Bitboard
  Bitboard queens = pos.getPiecesBB(us, QUEEN);
  // Rooks Bitboard (Diagonal pinned rooks cannot move)
  Bitboard rooks = pos.getPiecesBB(us, ROOK) & ~bishopPin;
  // Pinned rooks
  Bitboard pinned = (rooks | queens) & rookPin;
  // Non pinned rooks
  Bitboard nonPinned = rooks & ~rookPin;

  while (pinned) {
    // Get origin square and pop it from the bitboard
    Square origin = popLSB(pinned);
    // Generate rook attacks (Pinned rooks)
    Bitboard attacks =
        attacksBB<ROOK>(origin, pos.getOccupiedBB()) & available & rookPin;

    if constexpr (gt == CAPTURES)
      // Generate only attacks if in captures mode
      attacks &= pos.getOccupiedBB(them);
    if constexpr (gt == QUIETS)
      // Generate only non captures if in quiet mode
      attacks &= ~pos.getOccupiedBB();

    // Add moves to move list
    while (attacks) {
      Square destination = popLSB(attacks);
      *moves++ = Move::encode(origin, destination);
    }
  }

  while (nonPinned) {
    // Get origin square and pop it from the bitboard
    Square origin = popLSB(nonPinned);
    // Generate rook attacks (Non pinned rooks)
    Bitboard attacks = attacksBB<ROOK>(origin, pos.getOccupiedBB()) & available;

    if constexpr (gt == CAPTURES)
      // Generate only attacks if in captures mode
      attacks &= pos.getOccupiedBB(them);
    if constexpr (gt == QUIETS)
      // Generate only non captures if in quiet mode
      attacks &= ~pos.getOccupiedBB();

    // Add moves to move list
    while (attacks) {
      const Square destination = popLSB(attacks);
      *moves++ = Move::encode(origin, destination);
    }
  }

  return moves;
}

template <GenType gt>
Move *generateQueenMoves(Move *moves, const Position &pos) {
  // Side to move and enemy
  const Colour us = pos.getSideToMove();
  const Colour them = ~us;
  BoardState *st = pos.state();
  // masks
  Bitboard available = st->available;
  Bitboard rookPin = st->rookPin;
  Bitboard bishopPin = st->bishopPin;
  // Queens Bitboard (Non pinned)
  Bitboard queens = pos.getPiecesBB(us, QUEEN) & ~(bishopPin | rookPin);

  while (queens) {
    // Get origin square and pop it from the bitboard
    Square origin = popLSB(queens);
    // Generate queen non captures
    Bitboard attacks =
        attacksBB<QUEEN>(origin, pos.getOccupiedBB()) & available;

    if constexpr (gt == CAPTURES)
      attacks &= pos.getOccupiedBB(them);
    if constexpr (gt == QUIETS)
      attacks &= ~pos.getOccupiedBB();

    // Add moves to move list
    while (attacks) {
      const Square destination = popLSB(attacks);
      *moves++ = Move::encode(origin, destination);
    }
  }

  return moves;
}

template <GenType gt, Colour us>
Move *generateKingMoves(Move *moves, const Position &pos) {
  // Side to move and enemy
  constexpr Colour them = ~us;
  BoardState *st = pos.state();
  // King attacks
  Bitboard kingAttacks = st->kingAttacks;
  Square origin = getLSB(pos.getPiecesBB(us, KING));
  bool noCheck = (st->checkMask == FULLBB);

  if constexpr (gt == CAPTURES)
    // Generate only attacks if in captures mode
    kingAttacks &= pos.getOccupiedBB(them);
  if constexpr (gt == QUIETS)
    // Generate only non captures if in quiet mode
    kingAttacks &= ~pos.getOccupiedBB();

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
  constexpr Castling kingSide = (us == WHITE) ? WKCA : BKCA;
  constexpr Castling queenSide = (us == WHITE) ? WQCA : BQCA;

  if (st->castling & kingSide) {
    // Check if king is not in check and squares are not occupied
    if (noCheck && !(st->kingBan & kingSideSquares) &&
        !(pos.getOccupiedBB() & kingSideSquares))
      *moves++ = Move::encode<CASTLE>(origin, kingSideDest);
  }

  if (st->castling & queenSide) {
    // Check if king is not in check and squares are not occupied
    if (noCheck && !(st->kingBan & queenSideSquares) &&
        !(pos.getOccupiedBB() & queenSideOccupiedSquares))
      *moves++ = Move::encode<CASTLE>(origin, queenSideDest);
  }

  return moves;
}

template <GenType gt> Move *generateMoves(Move *moves, const Position &pos) {

  if (pos.getSideToMove() == WHITE)
    moves = generateKingMoves<gt, WHITE>(moves, pos);
  else
    moves = generateKingMoves<gt, BLACK>(moves, pos);

  if (pos.state()->checkMask == EMPTYBB)
    return moves;

  // Generate moves for each piece
  if (pos.getSideToMove() == WHITE)
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
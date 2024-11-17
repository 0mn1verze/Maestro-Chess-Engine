#include "pawns.hpp"
#include "defs.hpp"
#include "position.hpp"
#include "thread.hpp"
#include "utils.hpp"

namespace Maestro {

namespace Pawns {

/******************************************\
|==========================================|
|              Pawn Penalties              |
|==========================================|
\******************************************/

// Backward pawn penalty (If a backward pawn is a pawn that is behind all pawns
// of the same color on the adjacent files and cannot be safely advanced. -
// Wikipedia)
constexpr Score backwardPawn = _S(10, 25);
// Blocked storm penalty (If we allow our pawn storm to be blocked by the enemy,
// then the attack is effectively stopped, so it should be penalised greatly)
constexpr Score blockedStorm = _S(100, 100);
// Doubled pawn penalty (If a pawn is doubled, it is much less likely to become
// a queen as it kinda trips over itself)
constexpr Score doubledPawn = _S(10, 55);
// Isolated pawn penalty (If a pawn is isolated, it is likely to be a target
// during the middle game and is worth less than connected pawns, so the engine
// should be detered from creating isolated pawns)
constexpr Score isolatedPawn = _S(5, 15);
// Weak lever penalty (If a pawn is weak, it is likely to be a target during the
// middle game)
constexpr Score weakLever = _S(0, 55);
// Weak unopposed penalty (If a pawn is weak and unopposed, it is likely to be a
// target during the middle game)
constexpr Score weakUnopposed = _S(15, 30);

// Connected pawn bonus (If the connected pawns are closer to the promotion
// rank, they are worth more)
constexpr int connected[RANK_N] = {0, 5, 10, 15, 30, 50, 90};

// Strength of pawn shelter for our king by [distance from edge][rank].
// Used after castling
// Rank 1 is used for files where we have no pawn, or pawn is behind our king.
// Rank 8 is irrelevant as it is the last rank.

// Explanantion of the values:
// - The shelter strength is higher for the pawns closer to the king.
// - The shelter strength is higher for the pawns closer to the center.
constexpr Value shelterStrength[int(FILE_N) / 2][RANK_N] = {
    {-10, 15, 20, 10, 0, -5, -50},
    {-10, 15, 20, 10, 0, -5, -50},
    {-10, 15, 20, 10, 0, -5, -50},
    {-10, 15, 20, 10, 0, -5, -50}};

// Danger of enemy pawns moving toward our king by [distance from edge][rank].
// Rank 1 is used for files where the enemy has no pawn, or their pawn is behind
// our king. Rank 8 is irrelevant as it is the last rank.

// Explanantion of the values:
// - The danger is higher if their front most pawn is closer to the king.
// (Higher rank)
constexpr Value unblockedStorm[int(FILE_N) / 2][RANK_N] = {
    {0, -15, -10, 5, 10, 15, 20},
    {0, -15, -10, 5, 10, 15, 20},
    {0, -15, -10, 5, 10, 15, 20},
    {0, -15, -10, 5, 10, 15, 20}};

/******************************************\
|==========================================|
|                Evaluation                |
|! Heavily inspired by Stockfish 11's eval |
|==========================================|
\******************************************/

// Evaluate pawn structure
template <Colour us> Score evaluate(const Position &pos, Entry *entry) {
  constexpr Colour them = ~us;
  constexpr Direction up = pawnPush(us);

  Bitboard neighbours, stoppers, support, phalanx, opposed;
  Bitboard lever, leverPush, blocked, backup, doubled;
  bool backward, passed;

  Score score = SCORE_ZERO;

  const Square *pawns = pos.squares<PAWN>(us);
  const Bitboard ourPawns = pos.getPiecesBB(us, PAWN);
  const Bitboard theirPawns = pos.getPiecesBB(them, PAWN);

  Bitboard doubleAttacksThem = doublePawnAttacksBB<them>(theirPawns);

  entry->_passedPawns[us] = 0;
  entry->_kingSquare[us] = pos.square<KING>(us);
  entry->_pawnAttacks[us] = entry->_pawnAttacksSpan[us] =
      pawnAttacksBB<us>(ourPawns);

  Square sq;

  // Evaluate each pawn
  while ((sq = *pawns++) != NO_SQ) {

    Rank r = relativeRank(us, sq);
    // Neighbouring pawns
    neighbours = ourPawns & adjacentFilesBB(sq);
    // Blocked pawns
    blocked = theirPawns & (sq + up);
    // Backup pawns
    backup = ourPawns & passedPawnSpan(them, sq);
    // Opposed pawns
    opposed = theirPawns & forwardFileBB(us, sq);
    // Stopper pawns
    stoppers = theirPawns & passedPawnSpan(us, sq);
    // Lever pawns
    lever = theirPawns & pawnAttacksBB<us>(sq);
    // Lever push pawns
    leverPush = theirPawns & pawnAttacksBB<us>(sq + up);
    // Doubled pawns
    doubled = ourPawns & (sq - up);
    // Phalanx pawns (Pawns on the same rank)
    phalanx = neighbours & rankBB(sq);
    // Support pawns
    support = neighbours & rankBB(sq - up);

    // Backward pawns (No neighbours behind the pawn and cannot safely advance,
    // i.e blocked or approaching lever after push)
    backward =
        !(neighbours & forwardRanksBB(them, sq + up)) && (leverPush | blocked);

    // Pawn is passed if one of the following conditions is true:
    // 1. No stoppers except some levers (Can push through to be a passed pawn)
    // 2. Lever push but we outnumber the stoppers
    // 3. One front stopper that can be levered
    passed = !(stoppers ^ lever) ||
             (!(stoppers ^ leverPush) &&
              countBits(phalanx) > countBits(leverPush)) ||
             (stoppers == blocked && r >= RANK_5 &&
              (shift<up>(support) & ~(theirPawns | doubleAttacksThem)));

    // Update passed pawn bitboard
    if (passed)
      entry->_passedPawns[us] |= sq;

    // Calculate pawn score

    // If the pawn has support or could be supported
    if (support | phalanx) {
      // Pawn Score (Increase score for support and phalanx, but decrease for
      // opposition)
      Value v = connected[r] * (2 + bool(phalanx) - bool(opposed)) +
                20 * countBits(support);

      score += _S(v, v * (r - 2) / 4);
    }
    // Isolated pawns
    else if (!neighbours) {
      // Weak unopposed pawns are targets during the middle game
      score -= isolatedPawn + weakUnopposed * !opposed;
    }
    // Backward pawns
    else if (backward) {
      // Weak unopposed pawns are targets during the middle game
      score -= backwardPawn + weakUnopposed * !opposed;
    }

    // If there are no supporting pawns and the pawn is doubled or is a weak
    // lever, decrease the score
    if (!support)
      score -= doubledPawn * doubled + weakLever * moreThanOne(lever);
  }

  return score;
}

// Evaluate the pawn shelter for king safety
template <Colour us>
Score Entry::eval_shelter(const Position &pos, Square king) const {
  constexpr Colour them = ~us;
  Score bonus;

  // All the pawns in the storm range (Between the king and the promotion rank)
  Bitboard stormPawns = pos.getPiecesBB(PAWN) & ~forwardRanksBB(them, king);
  Bitboard ourPawns = stormPawns & pos.getPiecesBB(us, PAWN);
  Bitboard theirPawns = stormPawns & pos.getPiecesBB(them, PAWN);

  // Shelter Strength
  File center = Utility::clamp(fileOf(king), FILE_B, FILE_G);

  Bitboard b;

  // Loop through files for king shelter evaluation
  for (File f = File(center - 1); f <= File(center + 1); ++f) {

    b = ourPawns & fileBB(f);
    Rank ourRank = b ? relativeRank(us, frontMostSquare(them, b)) : RANK_1;

    b = theirPawns & fileBB(f);
    Rank theirRank = b ? relativeRank(them, frontMostSquare(us, b)) : RANK_1;

    File d = f > FILE_E ? FILE_H - f : f;
    bonus += _S(shelterStrength[d][ourRank], 0);

    // If the storm is blocked
    if (ourRank && (ourRank == theirRank - 1))
      // Apply blocked storm penalty if the storm is blocked at Rank 3
      bonus -= blockedStorm * (theirRank == RANK_3);
    else
      // Apply unblocked storm bonus if the storm is not blocked
      bonus += _S(unblockedStorm[d][ourRank], 0);
  }

  return bonus;
}

// Evaluate the king safety for a colour
template <Colour us> Score Entry::eval_king_safety(const Position &pos) {
  Square ksq = pos.square<KING>(us);
  _kingSquare[us] = ksq;
  _castlingRights[us] = pos.getCastlingRights();

  Score shelter = eval_shelter<us>(pos, ksq);

  Castling queenSide = (us == WHITE) ? WQ_SIDE : BQ_SIDE;
  Castling kingSide = (us == WHITE) ? WK_SIDE : BK_SIDE;

  // If we can castle use the bonus after castling
  if (pos.canCastle(kingSide))
    shelter = std::max(shelter, eval_shelter<us>(pos, relativeSquare(us, G1)));
  if (pos.canCastle(queenSide))
    shelter = std::max(shelter, eval_shelter<us>(pos, relativeSquare(us, C1)));

  // Bring king near the closest pawn (In the endgame stage)
  Bitboard pawns = pos.getPiecesBB(us, PAWN);
  int minPawnDist = pawns ? 8 : 0;

  if (pawns & attacksBB<KING>(ksq, EMPTYBB))
    minPawnDist = 1;
  else {
    while (pawns) {
      Square sq = popLSB(pawns);
      minPawnDist = std::min(minPawnDist, distance(ksq, sq));
    }
  }

  return shelter - _S(0, 16 * minPawnDist);
}

template Score Entry::eval_king_safety<WHITE>(const Position &pos);
template Score Entry::eval_king_safety<BLACK>(const Position &pos);

/******************************************\
|==========================================|
|              Pawn Table Probe            |
|==========================================|
\******************************************/

Entry *probe(const Position &pos) {
  Key key = pos.getPawnKey();
  Entry *entry = pos.getThread()->pawns[key];

  if (entry->_key == key)
    return entry;

  entry->_key = key;
  entry->_scores[WHITE] = evaluate<WHITE>(pos, entry);
  entry->_scores[BLACK] = evaluate<BLACK>(pos, entry);

  return entry;
}

} // namespace Pawns

} // namespace Maestro
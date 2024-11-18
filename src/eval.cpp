#include <algorithm>
#include <iostream>
#include <sstream>

#include "defs.hpp"
#include "eval.hpp"
#include "nnue.hpp"
#include "position.hpp"
#include "uci.hpp"
#include "utils.hpp"

namespace Maestro::Eval {

Score psqt[PIECE_N][SQ_N];

// clang-format off
constexpr Score bonus[PIECE_TYPE_N][SQ_N] = {
    {}, // No piece
    {
    _S(0  ,0  ), _S(0  ,0  ), _S(0  ,0  ), _S(0  ,0  ), _S(0  ,0  ), _S(0  ,0  ), _S(0  ,0  ), _S(0  ,0  ), 
    _S(-35,13 ), _S(-1 ,8  ), _S(-20,8  ), _S(-23,10 ), _S(-15,13 ), _S(24 ,0  ), _S(38 ,2  ), _S(-22,-7 ), 
    _S(-26,4  ), _S(-4 ,7  ), _S(-4 ,-6 ), _S(-10,1  ), _S(3  ,0  ), _S(3  ,-5 ), _S(33 ,-1 ), _S(-12,-8 ), 
    _S(-27,13 ), _S(-2 ,9  ), _S(-5 ,-3 ), _S(12 ,-7 ), _S(17 ,-7 ), _S(6  ,-8 ), _S(10 ,3  ), _S(-25,-1 ), 
    _S(-14,32 ), _S(13 ,24 ), _S(6  ,13 ), _S(21 ,5  ), _S(23 ,-2 ), _S(12 ,4  ), _S(17 ,17 ), _S(-23,17 ), 
    _S(-6 ,94 ), _S(7  ,100), _S(26 ,85 ), _S(31 ,67 ), _S(65 ,56 ), _S(56 ,53 ), _S(25 ,82 ), _S(-20,84 ), 
    _S(98 ,178), _S(134,173), _S(61 ,158), _S(95 ,134), _S(68 ,147), _S(126,132), _S(34 ,165), _S(-11,187), 
    _S(0  ,0  ), _S(0  ,0  ), _S(0  ,0  ), _S(0  ,0  ), _S(0  ,0  ), _S(0  ,0  ), _S(0  ,0  ), _S(0  ,0  ), 
    }, // Pawn
    {
    _S(-105,-29), _S(-21,-51), _S(-58,-23), _S(-33,-15), _S(-17,-22), _S(-28,-18), _S(-19,-50), _S(-23,-64), 
    _S(-29,-42), _S(-53,-20), _S(-12,-10), _S(-3 ,-5 ), _S(-1 ,-2 ), _S(18 ,-20), _S(-14,-23), _S(-19,-44), 
    _S(-23,-23), _S(-9 ,-3 ), _S(12 ,-1 ), _S(10 ,15 ), _S(19 ,10 ), _S(17 ,-3 ), _S(25 ,-20), _S(-16,-22), 
    _S(-13,-18), _S(4  ,-6 ), _S(16 ,16 ), _S(13 ,25 ), _S(28 ,16 ), _S(19 ,17 ), _S(21 ,4  ), _S(-8 ,-18), 
    _S(-9 ,-17), _S(17 ,3  ), _S(19 ,22 ), _S(53 ,22 ), _S(37 ,22 ), _S(69 ,11 ), _S(18 ,8  ), _S(22 ,-18), 
    _S(-47,-24), _S(60 ,-20), _S(37 ,10 ), _S(65 ,9  ), _S(84 ,-1 ), _S(129,-9 ), _S(73 ,-19), _S(44 ,-41), 
    _S(-73,-25), _S(-41,-8 ), _S(72 ,-25), _S(36 ,-2 ), _S(23 ,-9 ), _S(62 ,-25), _S(7  ,-24), _S(-17,-52), 
    _S(-167,-58), _S(-89,-38), _S(-34,-13), _S(-49,-28), _S(61 ,-31), _S(-97,-27), _S(-15,-63), _S(-107,-99), 
    }, // Knight
    {
    _S(-33,-23), _S(-3 ,-9 ), _S(-14,-23), _S(-21,-5 ), _S(-13,-9 ), _S(-12,-16), _S(-39,-5 ), _S(-21,-17), 
    _S(4  ,-14), _S(15 ,-18), _S(16 ,-7 ), _S(0  ,-1 ), _S(7  ,4  ), _S(21 ,-9 ), _S(33 ,-15), _S(1  ,-27), 
    _S(0  ,-12), _S(15 ,-3 ), _S(15 ,8  ), _S(15 ,10 ), _S(14 ,13 ), _S(27 ,3  ), _S(18 ,-7 ), _S(10 ,-15), 
    _S(-6 ,-6 ), _S(13 ,3  ), _S(13 ,13 ), _S(26 ,19 ), _S(34 ,7  ), _S(12 ,10 ), _S(10 ,-3 ), _S(4  ,-9 ), 
    _S(-4 ,-3 ), _S(5  ,9  ), _S(19 ,12 ), _S(50 ,9  ), _S(37 ,14 ), _S(37 ,10 ), _S(7  ,3  ), _S(-2 ,2  ),
    _S(-16,2  ), _S(37 ,-8 ), _S(43 ,0  ), _S(40 ,-1 ), _S(35 ,-2 ), _S(50 ,6  ), _S(37 ,0  ), _S(-2 ,4  ),
    _S(-26,-8 ), _S(16 ,-4 ), _S(-18,7  ), _S(-13,-12), _S(30 ,-3 ), _S(59 ,-13), _S(18 ,-4 ), _S(-47,-14),
    _S(-29,-14), _S(4  ,-21), _S(-82,-11), _S(-37,-8 ), _S(-25,-7 ), _S(-42,-9 ), _S(7  ,-17), _S(-8 ,-24),
    }, // Bishop
    {
    _S(-19,-9 ), _S(-13,2  ), _S(1  ,3  ), _S(17 ,-1 ), _S(16 ,-5 ), _S(7  ,-13), _S(-37,4  ), _S(-26,-20),
    _S(-44,-6 ), _S(-16,-6 ), _S(-20,0  ), _S(-9 ,2  ), _S(-1 ,-9 ), _S(11 ,-9 ), _S(-6 ,-11), _S(-71,-3 ),
    _S(-45,-4 ), _S(-25,0  ), _S(-16,-5 ), _S(-17,-1 ), _S(3  ,-7 ), _S(0  ,-12), _S(-5 ,-8 ), _S(-33,-16),
    _S(-36,3  ), _S(-26,5  ), _S(-12,8  ), _S(-1 ,4  ), _S(9  ,-5 ), _S(-7 ,-6 ), _S(6  ,-8 ), _S(-23,-11),
    _S(-24,4  ), _S(-11,3  ), _S(7  ,13 ), _S(26 ,1  ), _S(24 ,2  ), _S(35 ,1  ), _S(-8 ,-1 ), _S(-20,2  ),
    _S(-5 ,7  ), _S(19 ,7  ), _S(26 ,7  ), _S(36 ,5  ), _S(17 ,4  ), _S(45 ,-3 ), _S(61 ,-5 ), _S(16 ,-3 ),
    _S(27 ,11 ), _S(32 ,13 ), _S(58 ,13 ), _S(62 ,11 ), _S(80 ,-3 ), _S(67 ,3  ), _S(26 ,8  ), _S(44 ,3  ),
    _S(32 ,13 ), _S(42 ,10 ), _S(32 ,18 ), _S(51 ,15 ), _S(63 ,12 ), _S(9  ,12 ), _S(31 ,8  ), _S(43 ,5  ),
    }, // Rook
    {
    _S(-1 ,-33), _S(-18,-28), _S(-9 ,-22), _S(10 ,-43), _S(-15,-5 ), _S(-25,-32), _S(-31,-20), _S(-50,-41),
    _S(-35,-22), _S(-8 ,-23), _S(11 ,-30), _S(2  ,-16), _S(8  ,-16), _S(15 ,-23), _S(-3 ,-36), _S(1  ,-32),
    _S(-14,-16), _S(2  ,-27), _S(-11,15 ), _S(-2 ,6  ), _S(-5 ,9  ), _S(2  ,17 ), _S(14 ,10 ), _S(5  ,5  ),
    _S(-9 ,-18), _S(-26,28 ), _S(-9 ,19 ), _S(-10,47 ), _S(-2 ,31 ), _S(-4 ,34 ), _S(3  ,39 ), _S(-3 ,23 ),
    _S(-27,3  ), _S(-27,22 ), _S(-16,24 ), _S(-16,45 ), _S(-1 ,57 ), _S(17 ,40 ), _S(-2 ,57 ), _S(1  ,36 ),
    _S(-13,-20), _S(-17,6  ), _S(7  ,9  ), _S(8  ,49 ), _S(29 ,47 ), _S(56 ,35 ), _S(47 ,19 ), _S(57 ,9  ),
    _S(-24,-17), _S(-39,20 ), _S(-5 ,32 ), _S(1  ,41 ), _S(-16,58 ), _S(57 ,25 ), _S(28 ,30 ), _S(54 ,0  ),
    _S(-28,-9 ), _S(0  ,22 ), _S(29 ,22 ), _S(12 ,27 ), _S(59 ,27 ), _S(44 ,19 ), _S(43 ,10 ), _S(45 ,20 ),
    }, // Queen
    {
    _S(-15,-53), _S(36 ,-34), _S(12 ,-21), _S(-54,-11), _S(8  ,-28), _S(-28,-14), _S(24 ,-24), _S(14 ,-43),
    _S(1  ,-27), _S(7  ,-11), _S(-8 ,4  ), _S(-64,13 ), _S(-43,14 ), _S(-16,4  ), _S(9  ,-5 ), _S(8  ,-17),
    _S(-14,-19), _S(-14,-3 ), _S(-22,11 ), _S(-46,21 ), _S(-44,23 ), _S(-30,16 ), _S(-15,7  ), _S(-27,-9 ),
    _S(-49,-18), _S(-1 ,-4 ), _S(-27,21 ), _S(-39,24 ), _S(-46,27 ), _S(-44,23 ), _S(-33,9  ), _S(-51,-11),
    _S(-17,-8 ), _S(-20,22 ), _S(-12,24 ), _S(-27,27 ), _S(-30,26 ), _S(-25,33 ), _S(-14,26 ), _S(-36,3  ),
    _S(-9 ,10 ), _S(24 ,17 ), _S(2  ,23 ), _S(-16,15 ), _S(-20,20 ), _S(6  ,45 ), _S(22 ,44 ), _S(-22,13 ),
    _S(29 ,-12), _S(-1 ,17 ), _S(-20,14 ), _S(-7 ,17 ), _S(-8 ,17 ), _S(-4 ,38 ), _S(-38,23 ), _S(-29,11 ),
    _S(-65,-74), _S(23 ,-35), _S(16 ,-18), _S(-15,-18), _S(-56,-11), _S(-34,15 ), _S(2  ,4  ), _S(13 ,-17),
    }, // King
};

// clang-format on

// General evaluation terms
constexpr Score closeToKing = _S(10, 10);    // For minor pieces
constexpr Score minorBehindPawn = _S(5, 20); // For minor pieces
constexpr Score hanging = _S(70, 40);
constexpr Score restrictedPiece = _S(10, 10);
constexpr Score sliderOnQueen = _S(60, 20);

// Knight evaluation terms
constexpr Score knightOutpost = _S(10, 20);
constexpr Score reachableOutpost = _S(5, 10);
constexpr Score knightOnQueen = _S(15, 10);

// Bishop evaluation terms
constexpr Score bishopPair = _S(50, 50);
constexpr Score bishopBlockedByPawns = _S(5, 5);
constexpr Score bishopOutpost = _S(20, 5);
constexpr Score bishopLongDiagonal = _S(20, 11);

// Rook evaluation terms
constexpr Score rookOnOpenFile = _S(30, 5);
constexpr Score rookOnSemiOpenFile = _S(15, 5);
constexpr Score rookOnQueenFile = _S(10, 5);
constexpr Score rookTrapped = _S(50, 10);
constexpr Score rookOn7th = _S(0, 30);

// Queen evaluation terms
constexpr Score queenWeak = _S(50, 15);

// Mobility Table [PieceType - 2][Mobility]
// Explanation of the values:
// - The higher the mobility, the better the scores for the piece
constexpr Score mobilityBonus[][28] = {
    {_S(-70, -100), _S(-30, -100), _S(-15, -40), _S(-5, -15), _S(5, -10),
     _S(10, 10), _S(20, 10), _S(30, 10), _S(40, 10)}, // Knights
    {_S(-70, -150), _S(-30, -100), _S(-10, -60), _S(0, -30), _S(10, -20),
     _S(20, -5), _S(20, 5), _S(20, 10), _S(20, 20), _S(25, 20), _S(25, 20),
     _S(40, 20), _S(40, 20), _S(70, 30)}, // Bishops
    {_S(-150, -120), _S(-50, -100), _S(-15, -60), _S(-10, -20), _S(-10, 0),
     _S(-10, 15), _S(-10, 25), _S(0, 30), _S(5, 30), _S(10, 35), _S(15, 40),
     _S(20, 40), _S(20, 50), _S(30, 50), _S(80, 50)}, // Rooks
    {_S(-60, -250), _S(-60, -250), _S(-60, -200), _S(-20, -200), _S(-5, -140),
     _S(0, -100),   _S(5, -50),    _S(5, -20),    _S(10, -15),   _S(10, 5),
     _S(15, 15),    _S(15, 30),    _S(15, 30),    _S(15, 30),    _S(15, 40),
     _S(15, 40),    _S(15, 40),    _S(15, 40),    _S(15, 40),    _S(15, 40),
     _S(65, 65),    _S(70, 70),    _S(75, 75),    _S(80, 80),    _S(85, 85),
     _S(90, 90),    _S(95, 95),    _S(100, 100)} // Queen
};

// King evaluation terms

constexpr Value kingAttackWeights[PIECE_TYPE_N] = {0, 0, 80, 50, 40, 10};
constexpr Score flankAttacks = _S(10, 0);
constexpr Score pawnlessFlank = _S(20, 100);
constexpr Value queenSafeCheck = 100;
constexpr Value rookSafeCheck = 100;
constexpr Value bishopSafeCheck = 50;
constexpr Value knightSafeCheck = 120;

// Passed Pawn Evaluation Terms
// Explanation of the values:
// - The further advanced the passed pawn, the better the scores
constexpr Score passedPawn[RANK_N] = {_S(0, 0),    _S(10, 30), _S(15, 30),
                                      _S(15, 40),  _S(60, 70), _S(170, 180),
                                      _S(280, 280)};
constexpr Score passedFile = _S(10, 10);

// Threat Evaluation Terms

constexpr Score threatByMinor[PIECE_TYPE_N] = {
    _S(0, 0), _S(10, 30), _S(60, 40), _S(80, 60), _S(90, 120), _S(90, 160)};

constexpr Score threatByRook[PIECE_TYPE_N] = {
    _S(0, 0), _S(5, 40), _S(40, 70), _S(40, 60), _S(0, 40), _S(50, 40)};

constexpr Score threatWeakPawn = _S(10, 30);
constexpr Score threatByKing = _S(30, 90);
constexpr Score threatByPawnPush = _S(15, 20);
constexpr Score threatBySafePawn = _S(170, 90);

constexpr Value tempo = 20;

/******************************************\
|==========================================|
|             Evaluation Class             |
|==========================================|
\******************************************/

class Evaluation {
public:
  Evaluation() = delete;
  explicit Evaluation(const Position &pos) : pos(pos) {}
  Evaluation &operator=(const Evaluation &) = delete;
  Value value();
  void trace();

private:
  template <Colour us> void init();
  template <Colour us, PieceType pt> Score pieces();
  template <Colour us> Score king() const;
  template <Colour us> Score threats() const;
  template <Colour us> Score passed() const;
  template <Colour us> Score space() const;

  const Position &pos;
  Pawns::Entry *pawnEntry;

  // Mobility area (Squares that are "controlled" by us)
  Bitboard mobilityArea[COLOUR_N];

  // Mobility scores
  Score mobility[COLOUR_N] = {SCORE_ZERO, SCORE_ZERO};

  // Squares attacked by a given colour
  Bitboard attackedBy[COLOUR_N][PIECE_TYPE_N];

  // Squares attacked by two pieces
  Bitboard attackedBy2[COLOUR_N];

  // Squares adjacent to the king (Plus some near squares)
  Bitboard kingRing[COLOUR_N];

  // Number of pieces of the given colour that attack a square in the king ring
  int kingAttackersCount[COLOUR_N];
  // Sum of the weights of pieces that attack a square in the kingRing of the
  // enemy king
  int kingAttackersWeight[COLOUR_N];
  // Number of attacks by the given colour to squares directly adjacent to enemy
  // king
  int kingAttacksCount[COLOUR_N];
};

/******************************************\
|==========================================|
|           Classical Evaluation           |
|   ! Heavily borrowed from Stockfish 11   |
|==========================================|
\******************************************/

// Init piece square table
void initEval() {
  for (PieceType pt = PAWN; pt <= KING; ++pt) {
    for (Square sq = A1; sq <= H8; ++sq) {
      psqt[toPiece(WHITE, pt)][sq] = bonus[pt][sq] + pieceBonus[pt];
      psqt[toPiece(BLACK, pt)][flipRank(sq)] = -psqt[toPiece(WHITE, pt)][sq];
    }
  }
}

// Init evaluation
template <Colour us> void Evaluation::init() {
  constexpr Colour them = ~us;
  constexpr Direction up = pawnPush(us);
  constexpr Direction down = -up;
  constexpr Bitboard lowRanks =
      (us == WHITE) ? (RANK_2BB | RANK_3BB) : (RANK_7BB | RANK_6BB);

  const Square ksq = pos.square<KING>(us);

  Bitboard doubleAttackedByPawns =
      doublePawnAttacksBB<us>(pos.getPiecesBB(us, PAWN));

  Bitboard blockedOrLowRank =
      pos.getPiecesBB(us, PAWN) & (lowRanks | shift<down>(pos.getOccupiedBB()));

  // Squares occupied by blocked or low rank pawns, by our king or queen, by
  // blockers to attacks on our king, or controlled by enemy pawns are excluded
  // from the mobility area
  mobilityArea[us] = ~(blockedOrLowRank | pos.getPiecesBB(us, KING, QUEEN) |
                       pos.state()->pinned[us] | pawnEntry->pawnAttacks(them));

  // Initalise attackedBy[] for kings and pawns
  attackedBy[us][KING] = attacksBB<KING>(ksq);
  attackedBy[us][PAWN] = pawnEntry->pawnAttacks(us);
  attackedBy[us][ALL_PIECES] = attackedBy[us][PAWN] | attackedBy[us][KING];
  attackedBy2[us] =
      doubleAttackedByPawns | (attackedBy[us][PAWN] & attackedBy[us][KING]);

  // Init king safety tables
  Square s = toSquare(Utility::clamp(fileOf(ksq), FILE_B, FILE_G),
                      Utility::clamp(rankOf(ksq), RANK_2, RANK_7));

  kingRing[us] = attacksBB<KING>(s) | s;

  kingAttackersCount[them] =
      countBits(kingRing[us] & pawnEntry->pawnAttacks(them));
  kingAttacksCount[them] = kingAttackersWeight[them] = 0;

  // Remove from KingRing[] squares attacked by enemy pawns twice
  kingRing[us] &= ~doubleAttackedByPawns;
}

template <Colour us, PieceType pt> Score Evaluation::pieces() {
  constexpr Colour them = ~us;
  constexpr Direction down = -pawnPush(us);
  constexpr Bitboard outPostRank = us == WHITE
                                       ? (RANK_4BB | RANK_5BB | RANK_6BB)
                                       : (RANK_5BB | RANK_4BB | RANK_3BB);

  const Square *pl = pos.squares<pt>(us);
  const BoardState *st = pos.state();
  const Square ourKing = pos.square<KING>(us);
  const Square theirKing = pos.square<KING>(them);

  Bitboard attacked, excluded, occupied, bb;
  Square sq;
  Score score = SCORE_ZERO;

  if constexpr (pt == BISHOP)
    excluded = pos.getPiecesBB(QUEEN) | pos.getPiecesBB(us, BISHOP);
  else if constexpr (pt == ROOK)
    excluded = pos.getPiecesBB(QUEEN) | pos.getPiecesBB(us, ROOK);

  attackedBy[us][pt] = 0;

  while ((sq = *pl++) != NO_SQ) {

    attacked = attacksBB<pt>(sq, pos.getOccupiedBB() ^ excluded);

    // If the piece is pinned then it can only attacked the squares along the
    // pin mask
    if (st->pinned[us] & sq)
      attacked &= lineBB[ourKing][sq];

    attackedBy2[us] |= attackedBy[us][ALL_PIECES] & attacked;
    attackedBy[us][pt] |= attacked;
    attackedBy[us][ALL_PIECES] |= attacked;

    if (attacked & kingRing[them]) {
      kingAttackersCount[us]++;
      kingAttackersWeight[us] += kingAttackWeights[pt];
      kingAttacksCount[us] += countBits(attacked & attackedBy[them][KING]);
    }

    int mob = countBits(attacked & mobilityArea[us]);

    mobility[us] += mobilityBonus[pt - 2][mob];

    // Knight and Bishop Evaluation
    if (pt == KNIGHT || pt == BISHOP) {
      // Calculate the squares that can be an outpost for the piece
      // Outpost square means a square that is protected by a pawn and not
      // attacked by a pawn
      bb = outPostRank & attackedBy[us][PAWN] &
           ~pawnEntry->pawnAttacksSpan(them);

      if (bb & sq)
        score += (pt == KNIGHT) ? knightOutpost : bishopOutpost;
      else if (pt == KNIGHT && (bb & attacked & ~pos.getOccupiedBB(us)))
        score += reachableOutpost;

      // Being right behind a pawn
      if (shift<down>(pos.getPiecesBB(PAWN)) & sq)
        score += minorBehindPawn;

      score -= closeToKing * distance(sq, ourKing);

      if (pt == BISHOP) {
        // Blocked pawns
        Bitboard blocked =
            pos.getPiecesBB(us, PAWN) & shift<down>(pos.getOccupiedBB());

        // Penalty for bishop that are blocked by pawns (Scaled by number of
        // pawns on the same colour squares and number of blocked center
        // squares)
        score -= bishopBlockedByPawns *
                 countBits(pos.getPiecesBB(us, PAWN) & sameColourSquares(sq)) *
                 (1 + countBits(blocked & CenterFiles));

        // Bonus for bishops on a long diagonal which can see both center
        // squares
        if (moreThanOne(attacksBB<BISHOP>(sq, pos.getPiecesBB(PAWN)) & Center))
          score += bishopLongDiagonal;
      }
    }

    // Rook Evaluation
    if (pt == ROOK) {
      // Bonus for rook on the same file as a queen
      if (fileBB(sq) & pos.getPiecesBB(QUEEN))
        score += rookOnQueenFile;

      // Bonus for rook on an open or semi open file
      if (!(pos.getPiecesBB(us, PAWN) & fileBB(sq)))
        score += (pos.getPiecesBB(them, PAWN) & fileBB(sq)) ? rookOnSemiOpenFile
                                                            : rookOnOpenFile;

      // Penalty when trapped by king (Especially when cannot castle)
      else if (mob <= 3) {
        File kf = fileOf(ourKing);
        // If the king is on the same castling side as the rook
        if ((kf < FILE_E) == (fileOf(sq) < kf))
          score -= rookTrapped * (1 + !pos.getCastlingRights(us));
      }
    }

    // Queen Evaluation
    if (pt == QUEEN) {
      // If a queen is part of a realtive pin or discovered attack, then apply
      // the weak queen penalty
      Bitboard queenPinners;
      if (pos.getSliderBlockers(pos.getPiecesBB(BISHOP, ROOK), sq,
                                queenPinners))
        score -= queenWeak;
    }
  }

  return score;
}

template <Colour us> Score Evaluation::king() const {
  constexpr Colour them = ~us;
  // White camp is the area where the white king and most pieces are located.
  // Black camp is the area where the black king and most pieces are located.
  constexpr Bitboard camp =
      us == WHITE ? forwardRanksBB(BLACK, toSquare(FILE_A, RANK_6))
                  : forwardRanksBB(WHITE, toSquare(FILE_A, RANK_3));

  Score score = pawnEntry->kingSafety<us>(pos);
  int kingDanger = 0;
  const Square ksq = pos.square<KING>(us);
  Bitboard unsafeChecks;

  // Attacked squares defended at most once by queen or king
  Bitboard weak = attackedBy[them][ALL_PIECES] & ~attackedBy2[us] &
                  (~attackedBy[us][ALL_PIECES] | attackedBy[us][KING] |
                   attackedBy[us][QUEEN]);

  // Analyse the squares threatened by enemy pieces that are not defended or
  // only defended by a queen or king and is attacked twice by the enemy
  Bitboard safe = ~pos.getOccupiedBB(them) &
                  (~attackedBy[us][ALL_PIECES] | (weak & attackedBy2[them]));

  // Slider piece attacks (Direct or xray through our queen)
  Bitboard possibleBishopAttacks =
      attacksBB<BISHOP>(ksq, pos.getOccupiedBB() ^ pos.getPiecesBB(us, QUEEN));
  Bitboard possibleRookAttacks =
      attacksBB<ROOK>(ksq, pos.getOccupiedBB() ^ pos.getPiecesBB(us, QUEEN));

  // Squares where a rook can attack the king (could pin a queen as
  // well which counts as an attack)
  Bitboard rookChecks = possibleRookAttacks & attackedBy[them][ROOK];

  // If there are safe checks, then increment the king danger value
  if (rookChecks & safe)
    kingDanger += rookSafeCheck;
  else
    unsafeChecks |= rookChecks;

  // Enemy queen safe checks (Exclude rook checks and squares protected by our
  // queen, as we can just trade) Rook checks are already counted and more
  // valuable
  Bitboard queenChecks = (possibleRookAttacks & possibleBishopAttacks) &
                         attackedBy[them][QUEEN] & safe &
                         ~attackedBy[us][QUEEN] & ~rookChecks;

  if (queenChecks)
    kingDanger += queenSafeCheck;

  // Enemy bishop safe checks (Queen checks already counted and more valuable)
  Bitboard bishopChecks =
      possibleBishopAttacks & attackedBy[them][BISHOP] & safe & ~queenChecks;

  if (bishopChecks)
    kingDanger += bishopSafeCheck;
  else
    unsafeChecks |= bishopChecks;

  // Enemy knight checks
  Bitboard knightChecks = attacksBB<KNIGHT>(ksq) & attackedBy[them][KNIGHT];

  if (knightChecks & safe)
    kingDanger += knightSafeCheck;
  else
    unsafeChecks |= knightChecks;

  // Find the squares that opponent attacks in our king flank, the squares which
  // they attack twice in that flank, and the squares we defend
  Bitboard enemyFlankAttacks =
      attackedBy[them][ALL_PIECES] & KingFlank[fileOf(ksq)] & camp;
  Bitboard enemyFlankAttacksTwice = enemyFlankAttacks & attackedBy2[them];
  Bitboard ourFlankDefense =
      attackedBy[us][ALL_PIECES] & KingFlank[fileOf(ksq)] & camp;

  int kingFlankAttack =
      countBits(enemyFlankAttacks) + countBits(enemyFlankAttacksTwice);
  int kingFlankDefense = countBits(ourFlankDefense);

  // clang-format off

  // Calculate King Danger (Based on Stockfish 11's Formula)
  kingDanger += kingAttackersCount[them] * kingAttackersWeight[them] // More king attackers and higher weight increases king danger
              + 200 * countBits(kingRing[us] & weak) // More dangerous if king ring is weak
              + 150 * countBits(unsafeChecks) // More dangerous if there are unsafe checks
              + 100 * countBits(pos.state()->pinned[us]) // More dangerous if there are pinned pieces
              + 70 * kingAttacksCount[them] // More dangerous if there are more king attacks
              + 3 * kingFlankAttack * kingFlankAttack / 10 // More dangerous if there are attacks on king flank
              + (mobility[them] - mobility[us]).first  // More dangerous if the enemy has more mobility
              - 900 * !pos.count<toPiece(them, QUEEN)>() // A lot less dangerous if the enemy queen are not present
              - 100 * bool(attackedBy[us][KNIGHT] & attackedBy[us][KING]) // Less dangerous if the knight can block some attacks
              - 5 * score.first / 10 // Less dangerous if king safety (Pawn structure evaluation) is higher
              - 5 * kingFlankDefense // Less danergous if we defend more squares on the king flank 
              + 40; // Bias value
  // clang-format on

  // Transform king danger units into a score, and apply a penalty
  if (kingDanger > 100)
    score -= _S(kingDanger * kingDanger / 4096, kingDanger / 16);

  // Penalise king on a pawnless flank
  if (!(pos.getPiecesBB(PAWN) & KingFlank[fileOf(ksq)]))
    score -= pawnlessFlank;

  // Penalise if king flank is under attack
  score -= flankAttacks * kingFlankAttack;

  return score;
}

template <Colour us> Score Evaluation::threats() const {
  constexpr Colour them = ~us;
  constexpr Direction up = pawnPush(us);
  constexpr Bitboard pushRank = relativeRank(us, toSquare(FILE_A, RANK_3));

  Bitboard threatened;
  Score score = SCORE_ZERO;

  // Non pawn enemies
  Bitboard nonPawnEnemies = pos.getOccupiedBB(them) & ~pos.getPiecesBB(PAWN);

  // Protected by pawn or they have more defenders
  Bitboard stronglyProtected =
      attackedBy[them][PAWN] | (attackedBy2[them] & ~attackedBy2[us]);

  // Non-pawn enemies, strongly protected
  Bitboard defended = nonPawnEnemies & stronglyProtected;

  // Weak enemies not strongly protected and under our attack
  Bitboard weak =
      pos.getOccupiedBB(them) & ~stronglyProtected & attackedBy[us][ALL_PIECES];

  // Bonus according to kind of attacking piece
  if (defended | weak) {
    // Enemy pieces threatened by minor pieces
    threatened =
        (defended | weak) & (attackedBy[us][KNIGHT] | attackedBy[us][BISHOP]);
    while (threatened)
      score += threatByMinor[pos.getPieceType(popLSB(threatened))];

    // Enemy pieces threatened by rook
    threatened = weak & attackedBy[us][ROOK];
    while (threatened)
      score += threatByRook[pos.getPieceType(popLSB(threatened))];

    if (weak & attackedBy[us][KING])
      score += threatByKing;

    // Hanging pieces (Not protected or we outnumber them)
    threatened = weak & (~attackedBy[them][ALL_PIECES] |
                         (nonPawnEnemies & attackedBy2[us]));

    score += hanging * countBits(threatened);
  }

  // Bonus for attacking the weak areas so that the enemy's move is restricted
  threatened = attackedBy[them][ALL_PIECES] & ~stronglyProtected &
               attackedBy[us][ALL_PIECES];

  score += restrictedPiece * countBits(threatened);

  // Protected or unattacked squares
  Bitboard safe = ~attackedBy[them][ALL_PIECES] | attackedBy[us][ALL_PIECES];

  // Bonus for attacking enemies with our safe pawns (Creating an attack)
  Bitboard safePawn = pos.getPiecesBB(us, PAWN) & safe;
  safePawn = pawnAttacksBB(us, safePawn) & nonPawnEnemies;
  score += threatBySafePawn * countBits(safePawn);

  // Bonus for attacking with a safe pawn push
  Bitboard safePawnPush =
      shift<up>(pos.getPiecesBB(us, PAWN)) & ~pos.getOccupiedBB();
  // Add double pawn pushes as well
  safePawnPush |= shift<up>(safePawnPush & pushRank) & ~pos.getOccupiedBB();
  // Keep safe pawn pushes (Cannot be taken or is safe)
  safePawnPush &= ~attackedBy[them][PAWN] & safe;

  // Bonus for safe pawn threats on the next move
  safePawnPush = pawnAttacksBB<us>(safePawnPush) & nonPawnEnemies;
  score += threatByPawnPush * countBits(safePawnPush);

  // Bonus for threats against enemy queen on the next moves
  if (pos.getPiecesBB(them, QUEEN)) {
    Square sq = pos.square<QUEEN>(them);
    safe = mobilityArea[us] & ~stronglyProtected;

    threatened = attackedBy[us][KNIGHT] & attacksBB<KNIGHT>(sq) & safe;

    score += knightOnQueen * countBits(threatened);

    threatened =
        safe & attackedBy2[us] &
        ((attackedBy[us][BISHOP] & attacksBB<BISHOP>(sq, pos.getOccupiedBB())) |
         (attackedBy[us][ROOK] & attacksBB<ROOK>(sq, pos.getOccupiedBB())));

    score += sliderOnQueen * countBits(threatened);
  }

  return score;
}

template <Colour us> Score Evaluation::passed() const {
  constexpr Colour them = ~us;
  constexpr Direction up = pawnPush(us);

  auto kingDistance = [&](Colour c, Square sq) {
    return std::min(distance(pos.square<KING>(c), sq), 5);
  };

  Score score = SCORE_ZERO;

  Bitboard passedPawns = pawnEntry->passedPawns(us);

  while (passedPawns) {
    Square sq = popLSB(passedPawns);

    Rank rank = relativeRank(us, sq);

    Score bonus = passedPawn[rank];

    if (rank > RANK_3) {

      int w = 5 * rank - 13; // Factor that increases if the pawn is more
                             // advanced (starts at 2)
      Square blockSq = sq + up;
      // Adjust bonus based on the king's proximity
      // Asymmetry explained by the fact that if the opponent king is further
      // away from the block sq, it leads to a much higher chance of winning.
      // Even at the same distance, a passed pawn should have a decent bonus.
      bonus += _S(
          0, (5 * kingDistance(them, blockSq) - 2 * kingDistance(us, blockSq)) *
                 w);

      // Reduce bonus if the block sq is not the queening square by considering
      // a second push
      if (rank != RANK_7)
        bonus -= _S(0, kingDistance(us, blockSq + up) * w);

      // Increase bonus if pawn is free to advance
      if (pos.empty(blockSq)) {
        Bitboard squaresToQueen = forwardFileBB(us, sq);
        Bitboard unsafeSquares = passedPawnSpan(us, sq);

        // Rook or queen behind pawn
        Bitboard protection =
            forwardFileBB(them, sq) & pos.getPiecesBB(ROOK, QUEEN);

        // If there are no enemy rooks and queens behind our pawn, then only the
        // squares attacked by the enemy in front of the passed pawn is unsafe
        if (!(pos.getOccupiedBB(them) & protection))
          unsafeSquares &= attackedBy[them][ALL_PIECES];

        // If there are no enemy attacks on the passed pawn span, assign a huge
        // bonus. If there are no enemy attacks on the path to queen, assign a
        // smaller bonus. If it is safe to push the passed pawn forward, assign
        // a smaller bonus.
        int bonusFactor = !unsafeSquares                      ? 40
                          : !(unsafeSquares & squaresToQueen) ? 20
                          : !(unsafeSquares & blockSq)        ? 10
                                                              : 0;

        // Assign a larger bonus if the block square is defended
        if ((pos.getOccupiedBB(us) && protection) ||
            (attackedBy[us][ALL_PIECES] & blockSq))
          bonusFactor += 5;

        bonus += _S(bonusFactor * w);
      }
    }

    // Scale down bonus for candidate passers which need more than one pawn push
    // to become passed, or have a pawn in front of them
    if ((pos.getPiecesBB(them, PAWN) & passedPawnSpan(us, sq + up)) ||
        (pos.getPiecesBB(PAWN) & (sq + up)))
      bonus = bonus / 2;

    // Edge pass pawns are less valuable than center passed pawns
    score += bonus - passedFile * fileDistToEdge(sq);
  }

  return score;
}

/******************************************\
|==========================================|
|             Trace evaluation             |
|==========================================|
\******************************************/

void Evaluation::trace() {

  pawnEntry = Pawns::probe(pos);

  init<WHITE>();
  init<BLACK>();

  std::cout << "White Knight: " << score2Str(pieces<WHITE, KNIGHT>())
            << std::endl;
  std::cout << "Black Knight: " << score2Str(pieces<BLACK, KNIGHT>())
            << std::endl;
  std::cout << "White Bishop: " << score2Str(pieces<WHITE, BISHOP>())
            << std::endl;
  std::cout << "Black Bishop: " << score2Str(pieces<BLACK, BISHOP>())
            << std::endl;
  std::cout << "White Rook: " << score2Str(pieces<WHITE, ROOK>()) << std::endl;
  std::cout << "Black Rook: " << score2Str(pieces<BLACK, ROOK>()) << std::endl;
  std::cout << "White Queen: " << score2Str(pieces<WHITE, QUEEN>())
            << std::endl;
  std::cout << "Black Queen: " << score2Str(pieces<BLACK, QUEEN>())
            << std::endl;

  std::cout << "White King Score: " << score2Str(king<WHITE>()) << std::endl;
  std::cout << "Black King Score: " << score2Str(king<BLACK>()) << std::endl;

  std::cout << "White Threat Score: " << score2Str(threats<WHITE>())
            << std::endl;
  std::cout << "Black Threat Score: " << score2Str(threats<BLACK>())
            << std::endl;

  std::cout << "White Passed Score: " << score2Str(passed<WHITE>())
            << std::endl;
  std::cout << "Black Passed Score: " << score2Str(passed<BLACK>())
            << std::endl;
}

/******************************************\
|==========================================|
|              NNUE Evaluation             |
|             (Stockfish NNUE)             |
|==========================================|
\******************************************/

// convert BBC piece code to Stockfish piece codes
int nnuePieces[PIECE_N] = {blank, wpawn,  wknight, wbishop, wrook,   wqueen,
                           wking, blank,  blank,   bpawn,   bknight, bbishop,
                           brook, bqueen, bking,   blank};

int toNNUEPiece(Piece piece) { return nnuePieces[piece]; }

inline Value evaluate_nnue(const Position &pos) {
  Bitboard bitboard;
  Square sq;
  int pieces[33];
  int squares[33];
  int index = 0;

  // Set king squares
  pieces[index] = nnuePieces[wK];
  squares[index++] = pos.square<KING>(WHITE);

  pieces[index] = nnuePieces[bK];
  squares[index++] = pos.square<KING>(BLACK);

  for (PieceType pt = PAWN; pt <= QUEEN; ++pt) {
    const Square *wpl = pos.squares(WHITE, pt);
    while (*wpl != NO_SQ) {
      pieces[index] = nnuePieces[toPiece(WHITE, pt)];
      squares[index++] = *wpl++;
    }

    const Square *bpl = pos.squares(BLACK, pt);
    while (*bpl != NO_SQ) {
      pieces[index] = nnuePieces[toPiece(BLACK, pt)];
      squares[index++] = *bpl++;
    }
  }

  pieces[index] = 0;
  squares[index] = 0;

  if (pos.state()->plies > 2) {
    // Get previous nnue accumulator data
    NNUEdata *data[3];
    data[0] = &(pos.state()->nnueData);
    data[1] = &(pos.state()->previous->nnueData);
    data[2] = &(pos.state()->previous->previous->nnueData);

    return nnue_evaluate_incremental(pos.getSideToMove(), pieces, squares,
                                     data);
  } else
    return nnue_evaluate(pos.getSideToMove(), pieces, squares);
}

// Evaluate the position
Value evaluate(const Position &pos) {
  BoardState *st = pos.state();

  if constexpr (DEFAULT_USE_NNUE) {
    Value nnue = evaluate_nnue(pos);

    Value v = nnue * 5 / 4 + 28;

    v = v * (100 - st->fiftyMove) / 100;

    v = std::clamp(v, -VAL_MATE_BOUND + 1, VAL_MATE_BOUND - 1);

    return v / 2;
  } else {
    Score psq = pos.psq();

    int mgPhase = st->gamePhase;
    if (mgPhase > 24)
      mgPhase = 24;
    int egPhase = 24 - mgPhase;

    Value v = (psq.first * mgPhase + psq.second * egPhase) / 24;

    return pos.getSideToMove() == WHITE ? v : -v;
  }
}

void trace(const Position &pos) { Evaluation(pos).trace(); }

} // namespace Maestro::Eval
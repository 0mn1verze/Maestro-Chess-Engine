#include <format>
#include <iostream>
#include <string>

#include "bitboard.hpp"
#include "defs.hpp"

/******************************************\
|==========================================|
|              Lookup Tables               |
|==========================================|
\******************************************/

// Pseudo attacks for all pieces except pawns
Bitboard pseudoAttacks[PIECE_TYPE_N][SQ_N];
// Pseudo attacks for pawns
Bitboard pawnAttacks[COLOUR_N][SQ_N];
// Bishop magics
Magic bishopAttacks[SQ_N]{};
// Rook magics
Magic rookAttacks[SQ_N]{};
// Bishop attack tables
Bitboard bishopTable[0x1480]{};
// Rook attack tables
Bitboard rookTable[0x19000]{};
// Square distance lookup table
int dist[SQ_N][SQ_N];
// Line the two squares lie on [from][to]
Bitboard lineBB[SQ_N][SQ_N];
// In between bitboards [from][to]
Bitboard betweenBB[SQ_N][SQ_N];
// Pins [king][attacker]
Bitboard pinBB[SQ_N][SQ_N];
// Checks [king][attacker]
Bitboard checkBB[SQ_N][SQ_N];
// Castling rights lookup table
Castling castlingRights[SQ_N];

/******************************************\
|==========================================|
|             Bitboard print               |
|==========================================|
\******************************************/

// Bitboard print function
void printBitboard(Bitboard bb) {
  std::string sep{"\n     +---+---+---+---+---+---+---+---+\n"};
  std::cout << sep;
  for (Rank r = RANK_8; r >= RANK_1; --r) {
    // Print rank number
    std::cout << " " << (r + 1) << "   |";
    // Print squares
    for (File f = FILE_A; f <= FILE_H; ++f) {
      std::cout << ((bb & squareBB(toSquare(f, r))) ? " 1 " : " . ") << "|";
    }
    std::cout << sep;
  }
  std::cout << std::endl;

  // Print file letters
  std::cout << "       A   B   C   D   E   F   G   H\n\n";

  // Print bitboard representations
  std::cout << std::format("Bitboard: {:#x}", bb) << std::endl;
}

/******************************************\
|==========================================|
|            Attacks on the fly            |
|==========================================|
\******************************************/

// Calculate the attacks of a piece on the fly (Slow approach used to populate
// tables)
template <PieceType pt>
static inline Bitboard attacksOnTheFly(Square sq, Bitboard occupied) {
  // Directions for rook and bishop
  constexpr Direction rookDir[4] = {N, E, W, S};
  constexpr Direction bishopDir[4] = {NE, NW, SE, SW};

  // Get the corresponding attack pattern for the piece
  Bitboard attacks = EMPTYBB;
  for (Direction d : (pt == BISHOP) ? bishopDir : rookDir) {
    Square to = sq;
    // While the shift is valid and the square is not occupied
    while (shift(to, d) && !(occupied & to))
      attacks |= (to += d);
  }

  return attacks;
}

/******************************************\
|==========================================|
|               Init Magics                |
|==========================================|
\******************************************/

template <PieceType pt>
constexpr void initMagics(Bitboard table[], Magic magics[]) {
  int size;
  Bitboard edges, occupied;

  for (Square sq = A1; sq <= H8; ++sq) {
    // Edges of the board are not included in the attack lookup because it
    // doesn't matter if there is a blocker there or not Except in the cases
    // where the attacking piece is on the edge.
    edges = ((rankBB(RANK_1) | rankBB(RANK_8)) & ~rankBB(sq)) |
            ((fileBB(FILE_A) | fileBB(FILE_H)) & ~fileBB(sq));

    // Get the corresponding magic bitboard entry
    Magic &m = magics[sq];
    // Generate attack mask (Remove edges)
    m.mask = attacksOnTheFly<pt>(sq, EMPTYBB) & ~edges;

    // Assign the attacks pointer to the corresponding index in the attacks
    // table The table is like a 2D array but each row has different length
    // correspoding to the number of possible subset of bits in a mask
    m.attacks = (sq == A1) ? table : magics[sq - 1].attacks + size;

    // Reset occupied and size as we are starting to fill the m.attacks array
    occupied = EMPTYBB;
    size = 0;
    do {
      // Using the index function to calculate the corresponding index for the
      // occupancy bitboard, and generate the corresponding attacks (with
      // blockers) and store it at that index in the table
      m.attacks[m.index(occupied)] = attacksOnTheFly<pt>(sq, occupied);

      // Increment size
      size++;
      // Carry Rippler trick from
      // https://www.chessprogramming.org/Traversing_Subsets_of_a_Set
      occupied = (occupied - m.mask) & m.mask;
    } while (occupied);
  }
}

/******************************************\
|==========================================|
|              Init Bitboards              |
|==========================================|
\******************************************/

// initialize bitboards lookup tables
void initBitboards() {
  // Init square distances
  for (Square sq1 = A1; sq1 <= H8; ++sq1)
    for (Square sq2 = A1; sq2 <= H8; ++sq2)
      dist[sq1][sq2] = std::max(static_cast<int>(rankDist(sq1, sq2)),
                                static_cast<int>(fileDist(sq1, sq2)));

  // Init magics bitboards
  initMagics<BISHOP>(bishopTable, bishopAttacks);
  initMagics<ROOK>(rookTable, rookAttacks);

  // Init pseudo attacks for knights and kings
  for (Square sq = A1; sq <= H8; ++sq) {
    // Init pseudo attacks for pawn
    pawnAttacks[WHITE][sq] = shift<NW>(squareBB(sq)) | shift<NE>(squareBB(sq));
    pawnAttacks[BLACK][sq] = shift<SW>(squareBB(sq)) | shift<SE>(squareBB(sq));
    // Init pseudo attacks for knights
    for (Direction d : {NNE, NNW, NEE, NWW, SEE, SWW, SSE, SSW})
      pseudoAttacks[KNIGHT][sq] |= shift(sq, d);
    // Init pseudo attacks for king
    for (Direction d : {N, NE, NW, E, W, SE, SW, S})
      pseudoAttacks[KING][sq] |= shift(sq, d);
    // Init pseudo attacks for bishop
    pseudoAttacks[BISHOP][sq] = bishopAttacks[sq][EMPTYBB];
    pseudoAttacks[ROOK][sq] = rookAttacks[sq][EMPTYBB];
    pseudoAttacks[QUEEN][sq] =
        pseudoAttacks[BISHOP][sq] | pseudoAttacks[ROOK][sq];
  }

  for (Square from = A1; from <= H8; ++from) {
    // Get the squares attacked by the bishop
    Bitboard attacked = attacksBB<BISHOP>(from, EMPTYBB);

    // For each square attacked by the bishop, calculate the line it lies on
    while (attacked) {
      Square to = popLSB(attacked);
      lineBB[from][to] =
          (attacksBB<BISHOP>(from, EMPTYBB) & attacksBB<BISHOP>(to, EMPTYBB)) |
          (from | to);
      betweenBB[from][to] = attacksBB<BISHOP>(from, squareBB(to)) &
                            attacksBB<BISHOP>(to, squareBB(from));
      pinBB[from][to] = betweenBB[from][to] | to;
      checkBB[from][to] =
          betweenBB[from][to] | from | (from - direction(from, to));
    }

    // Get the squares attacked by the rook
    attacked = attacksBB<ROOK>(from, EMPTYBB);

    // For each square attacked by the bishop, calculate the line it lies on
    while (attacked) {
      Square to = popLSB(attacked);
      lineBB[from][to] =
          (attacksBB<ROOK>(from, EMPTYBB) & attacksBB<ROOK>(to, EMPTYBB)) |
          (from | to);
      betweenBB[from][to] = attacksBB<ROOK>(from, squareBB(to)) &
                            attacksBB<ROOK>(to, squareBB(from));
      pinBB[from][to] = betweenBB[from][to] | to;
      checkBB[from][to] =
          betweenBB[from][to] | from | (from - direction(from, to));
    }
  }

  // Init castling rights array
  std::fill_n(castlingRights, std::size(castlingRights), ANY_SIDE);
  // Special cases
  // Remove White castling rights if the king is moved
  castlingRights[E1] &= ~WHITE_SIDE;
  // Remove Black castling rights if the king is moved
  castlingRights[E8] &= ~BLACK_SIDE;
  // Remove White King side castling rights if the rook on H1 is moved
  castlingRights[H1] &= ~WK_SIDE;
  // Remove White Queen side castling rights if the rook on A1 is moved
  castlingRights[A1] &= ~WQ_SIDE;
  // Remove Black King side castling rights if the rook on H8 is moved
  castlingRights[H8] &= ~BK_SIDE;
  // Remove Black Queen side castling rights if the rook on A8 is moved
  castlingRights[A8] &= ~BQ_SIDE;
}

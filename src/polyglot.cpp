#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "defs.hpp"
#include "movegen.hpp"
#include "polyglot.hpp"
#include "position.hpp"
#include "utils.hpp"

PolyBook book;

void initPolyBook(std::string filename) {
  book.entries.clear();
  std::ifstream file(filename, std::ios::binary);
  if (file.is_open()) {
    auto size = std::filesystem::file_size(filename);
    book.num_poly_entries = size / sizeof(PolyBookEntry);
    book.entries.resize(book.num_poly_entries);
    file.read(reinterpret_cast<char *>(book.entries.data()), size);
    file.close();
  } else
    std::cout << "Error: Could not open polyglot book file (" << filename << ")"
              << std::endl;
}

void clearPolyBook() { book.entries.clear(); }

Key getPolyKey(const Position &pos) {
  int offset = 0;
  int polyPiece = NO_PIECE;
  Piece piece = NO_PIECE;

  Key key = 0ULL;

  for (Square sq = A1; sq <= H8; ++sq) {
    piece = pos.getPiece(sq);
    if (piece != NO_PIECE)
      key ^= Random64Poly[(64 * toPolyPiece[piece]) + sq];
  }

  offset = 768;

  if (pos.canCastle(WK_SIDE))
    key ^= Random64Poly[offset + 0];
  if (pos.canCastle(WQ_SIDE))
    key ^= Random64Poly[offset + 1];
  if (pos.canCastle(BK_SIDE))
    key ^= Random64Poly[offset + 2];
  if (pos.canCastle(BQ_SIDE))
    key ^= Random64Poly[offset + 3];

  offset = 772;

  Colour us = pos.getSideToMove();
  Colour enemy = ~us;
  Square enPassant = pos.state()->enPassant;
  bool enPassantAvailable =
      (us == WHITE)
          ? pawnAttacksBB<BLACK>(enPassant) & pos.getPiecesBB(WHITE, PAWN)
          : pawnAttacksBB<WHITE>(enPassant) & pos.getPiecesBB(BLACK, PAWN);

  if (enPassant != NO_SQ and enPassantAvailable)
    key ^= Random64Poly[offset + fileOf(enPassant)];

  if (us == WHITE)
    key ^= Random64Poly[780];

  return key;
}

constexpr char promotedPieceASCII[] = "nbrq";

GenMove polyMoveToEngineMove(const Position &pos, std::uint16_t polyMove) {

  File from_file = File((polyMove >> 6) & 7);
  Rank from_rank = Rank((polyMove >> 9) & 7);
  File to_file = File((polyMove >> 0) & 7);
  Rank to_rank = Rank((polyMove >> 3) & 7);
  int promotedPiece = (polyMove >> 12) & 7;

  Square from_square = toSquare(from_file, from_rank);
  Square to_square = toSquare(to_file, to_rank);

  std::string move;

  move += sq2Str(from_square) + sq2Str(to_square);

  if (promotedPiece > 0) {
    move += promotedPieceASCII[promotedPiece];
  }

  // Generate all moves
  GenMove moves[256];
  GenMove *last = generateMoves<ALL>(moves, pos);

  for (GenMove *begin = moves; begin < last; ++begin) {
    if (move == move2Str(*begin)) {
      return *begin;
    }
  }

  return GenMove::none();
}

GenMove getPolyBookMove(const Position &pos) {
  srand(getTimeMs());

  Key polyKey = getPolyKey(pos);

  GenMove bookMoves[32]{};

  int index = 0;
  for (const auto &entry : book.entries) {
    if (__builtin_bswap64(entry.key) == polyKey) {
      // Get move from the book and then convert to engine move format
      Move move = polyMoveToEngineMove(pos, __builtin_bswap16(entry.move));

      if (move.isValid())
        // Add moves to book moves
        bookMoves[index++] = move;
    }
  }

  if (index != 0) {
    int randomIndex = rand();
    return bookMoves[randomIndex % index];
  }

  return GenMove::none();
}
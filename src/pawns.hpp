#ifndef PAWNS_HPP
#define PAWNS_HPP

#include "defs.hpp"
#include "position.hpp"
#include "utils.hpp"

namespace Maestro {

namespace Pawns {

struct Entry {

  // Getters
  Score pawn_score(Colour c) const { return _scores[c]; }
  Bitboard pawnAttacks(Colour c) const { return _pawnAttacks[c]; }
  Bitboard pawnAttacksSpan(Colour c) const { return _pawnAttacksSpan[c]; }
  Bitboard passedPawns(Colour c) const { return _passedPawns[c]; }
  int passedCount(Colour c) const { return _passedPawns[c]; }

  template <Colour us> Score king_safety(const Position &pos) const {
    return _kingSquare[us] == pos.square<KING>(us) &&
                   _castlingRights[us] == pos.getCastlingRights()
               ? _kingSafety[us]
               : eval_king_safety<us>(pos);
  }

  // Variables
  Key _key;
  Score _scores[COLOUR_N];
  Bitboard _passedPawns[COLOUR_N];
  Bitboard _pawnAttacks[COLOUR_N];
  Bitboard _pawnAttacksSpan[COLOUR_N];
  Square _kingSquare[COLOUR_N];
  Score _kingSafety[COLOUR_N];
  Castling _castlingRights[COLOUR_N];

private:
  template <Colour us> Score eval_king_safety(const Position &pos);

  template <Colour us>
  Score eval_shelter(const Position &pos, Square king) const;
};

using Table = HashTable<Entry, 131072>;

Entry *probe(const Position &pos, Pawns::Table &table);

} // namespace Pawns

} // namespace Maestro

#endif // PAWNS_HPP
#ifndef PAWNS_HPP
#define PAWNS_HPP

#include "defs.hpp"
#include "position.hpp"
#include "utils.hpp"

namespace Maestro {

namespace Pawns {

struct Entry {};

using Table = HashTable<Entry, 131072>;

Entry *probe(const Position &pos);

} // namespace Pawns

} // namespace Maestro

#endif // PAWNS_HPP
#ifndef POLYGLOT_HPP
#pragma once
#define POLYGLOT_HPP

#include <string>

#include "defs.hpp"
#include "move.hpp"
#include "position.hpp"

namespace Maestro {

// Polyglot book class
class PolyBook {
public:
  void init(const std::string_view &path);
  void clear();

  Move probe(const Position &pos) const;

private:
  Key polyKey(const Position &pos) const;
  Move polyMoveToEngineMove(const Position &pos, U16 polyMove) const;

  struct Entry {
    Key key;
    U16 move;
    U16 weight;
    U32 learn;
  };

  size_t _size;
  std::vector<Entry> _entries;
};

} // namespace Maestro

#endif
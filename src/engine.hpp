#ifndef ENGINE_HPP
#pragma once
#define ENGINE_HPP

#include <string>
#include <vector>

#include "defs.hpp"
#include "polyglot.hpp"
#include "position.hpp"
#include "search.hpp"
#include "thread.hpp"
#include "utils.hpp"

namespace Maestro {

/******************************************\
|==========================================|
|               Engine Class               |
|==========================================|
\******************************************/

class Engine {
public:
  Engine();
  ~Engine();

  void waitForSearchFinish();
  void setPosition(const std::string fen,
                   const std::vector<std::string> &moves);

  void perft(Limits &limits);
  void bench();
  void go(Limits &limits);
  void stop();
  bool stopped() const { return threads.stop; }
  void clear();

  std::string fen() const;
  void print() const;

  void setOption(const std::string &name, const std::string &value);

private:
  Position pos;
  StateListPtr states;
  PolyBook book;

  ThreadPool threads;
  TTable tt;
  SearchState searchState{threads, tt};
};

} // namespace Maestro

#endif // ENGINE_HPP
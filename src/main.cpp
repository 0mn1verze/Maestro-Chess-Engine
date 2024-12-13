#include <cstring>
#include <iostream>

#include "movepicker.hpp"
#include "perft.hpp"
#include "uci.hpp"

using namespace Maestro;

int main() {

  UCI uci;

  uci.loop();

  // std::istringstream is("go depth 20");
  // uci.go(is);

  return 0;
}
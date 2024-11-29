#include <iostream>

#include "search.hpp"
#include "uci.hpp"

namespace Maestro {

void TimeManager::init(Limits &limits, Colour us, int ply) {

  startTime = limits.startTime;
  // If we have no time, we don't need to continue
  if (limits.time[us] == 0)
    return;

  const TimePt time = limits.time[us];
  const TimePt inc = limits.inc[us];

  int mtg = limits.movesToGo ? std::min(limits.movesToGo, 50) : 50;

  if (limits.movesToGo > 0) {
    // X / Y + Z time management
    optimumTime = 1.50 * (time - MOVE_OVERHEAD) / mtg + inc;
    maximumTime = 3.00 * (time - MOVE_OVERHEAD) / mtg + inc;
  } else {
    // X + Y time management
    optimumTime = 1.50 * (time - MOVE_OVERHEAD + 25 * inc) / 50;
    maximumTime = 3.00 * (time - MOVE_OVERHEAD + 25 * inc) / 50;
  }

  // Cap time allocation using move overhead
  optimumTime = std::min(optimumTime, time - MOVE_OVERHEAD);
  maximumTime = std::min(maximumTime, time - MOVE_OVERHEAD);

  std::cout << "Optimum: " << optimumTime << std::endl;
  std::cout << "Maximum: " << maximumTime << std::endl;
}

void SearchWorker::startSearch() {
  std::cout << "Starting search" << std::endl;

  if (!isMainThread()) {
    return;
  }

  threads.startSearch();
}

void SearchWorker::clear() {}

} // namespace Maestro
#include "search.hpp"

namespace Maestro {

void SearchWorker::clear() {
  killerTable.fill(Move::none());
  counterMoveTable.fill(Move::none());
  historyTable.fill(0);
  captureHistoryTable.fill(0);

  for (bool isCapture : {false, true})
    for (PieceType pt = PAWN; pt <= KING; ++pt)
      for (Square to = A1; to <= H8; ++to)
        for (int cont : {0, 1})
          continuationTable[isCapture][pt][to].fill(
              StatsEntry<ContinuationHistory, NOT_USED>{});
}

void SearchWorker::startSearch() {}
} // namespace Maestro
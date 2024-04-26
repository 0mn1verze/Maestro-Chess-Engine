#include <algorithm>
#include <cmath>
#include <cstring>
#include <iostream>
#include <iterator>
#include <sstream>

#include "eval.hpp"
#include "hashtable.hpp"
#include "movepicker.hpp"
#include "perft.hpp"
#include "search.hpp"

int LMRTable[64][64];
int LateMovePruningCounts[2][11];

// Futility margin (from Stockfish)
Value futility_margin(Depth d, bool noTtCutNode, bool improving,
                      bool oppWorsening) {
  Value futilityMult = 118 - 44 * noTtCutNode;
  Value improvingDeduction = 53 * improving * futilityMult / 32;
  Value worseningDeduction =
      (309 + 47 * improving) * oppWorsening * futilityMult / 1024;

  return futilityMult * d - improvingDeduction - worseningDeduction;
}

// Thread related stuff
SearchWorker::~SearchWorker() {
  quit = true;
  startSearch();
  searchThread.join();
}

void SearchWorker::idleLoop() {
  while (true) {
    std::unique_lock<std::mutex> lock(mutex);
    searching = false;
    cv.notify_one(); // Check for search finished
    cv.wait(lock, [this] { return searching; });

    if (quit) {
      return;
    }

    lock.unlock();
    stop = false;
    searchPosition();
  }
}

void SearchWorker::startSearch() {
  mutex.lock();
  searching = true;
  mutex.unlock();
  cv.notify_one(); // Wake up idle loop thread
}

void SearchWorker::waitForSearchFinished() {
  std::unique_lock<std::mutex> lock(mutex);
  cv.wait(lock, [this] { return !searching; });
}

void SearchWorker::start(Position &pos, StateList &states, TimeControl &time,
                         Depth depth) {

  waitForSearchFinished();
  rootState = *pos.state();
  rootPos = pos;

  Colour us = pos.getSideToMove();

  nodes = 0;
  tm = time;
  maxDepth = depth;
  pvStability = 0;
  ply = 0;

  std::fill(results, results + MAX_DEPTH + 1, Result{});
  std::fill(nodeStates, nodeStates + MAX_DEPTH + 1, NodeState{});

  tm.startTime = getTimeMs();

  startSearch();
}

void SearchWorker::checkTime() {
  if (tm.idealStopTime == 0) {
    return;
  }

  if (finishSearch()) {
    stop = true;
  }
}

bool SearchWorker::finishSearch() {
  if (currentDepth <= 4)
    return false;

  Time elapsed = tm.elapsed();

  if (elapsed > tm.maxStopTime)
    return true;

  const double pvFactor = 1.20 - 0.04 * pvStability;

  const double scoreChange =
      results[currentDepth].best - results[currentDepth - 3].best;

  const double scoreFactor = std::max(0.75, std::max(1.25, scoreChange * 0.05));

  return elapsed > tm.idealStopTime * scoreFactor * pvFactor * 0.5;
}

void SearchWorker::tmUpdate() {
  pvStability =
      (results[currentDepth].pv.line[0] == results[currentDepth - 1].pv.line[0])
          ? std::min(10, pvStability + 1)
          : 0;
}

std::string SearchWorker::pv(PVLine pvLine) {
  std::stringstream ss;

  Time time = tm.elapsed() + 1;
  int hashFull = TTHashFull();

  Value score = results[currentDepth].best;

  ss << "info"
     << " depth " << currentDepth << " seldepth " << selDepth << " score "
     << score2Str(score) << " nodes " << nodes << " nps " << nodes * 1000 / time
     << " hashfull " << hashFull << " tbhits " << 0 << " time " << time
     << " pv";

  if (score > -VAL_MATE and score < -VAL_MATE_BOUND) {
    pvLine.length = score + VAL_MATE;
  } else if (score < VAL_MATE and score > VAL_MATE_BOUND) {
    pvLine.length = VAL_MATE - score;
  }

  for (int index = 0; index < pvLine.length; index++) {
    ss << " " << move2Str(Move(pvLine.line[index]));
  }

  return ss.str();
}

static void updatePV(PVLine &parentPV, PVLine &childPV, Move &move) {
  parentPV.length = 1 + childPV.length;
  parentPV.line[0] = move;
  std::copy(childPV.line, childPV.line + childPV.length, parentPV.line + 1);
}

void initLMR() {

  // Init Late Move Reductions Table
  for (int depth = 1; depth < 64; depth++)
    for (int played = 1; played < 64; played++)
      LMRTable[depth][played] = 0.7844 + log(depth) * log(played) / 2.4696;

  for (int depth = 1; depth <= 10; depth++) {
    LateMovePruningCounts[0][depth] = 2.0767 + 0.3743 * depth * depth;
    LateMovePruningCounts[1][depth] = 3.8733 + 0.7124 * depth * depth;
  }
}

void SearchWorker::searchPosition() {
  // Iterative deepening
  TTUpdate();
  initLMR();
  iterativeDeepening();
  searching = false;
  waitForSearchFinished();
}

void SearchWorker::iterativeDeepening() {

  for (currentDepth = 1; currentDepth <= maxDepth; currentDepth++) {

    Result &r = results[currentDepth];
    aspirationWindow();

    if (stop)
      break;
    tmUpdate();
  }
  std::cout << "bestmove " << move2Str(results[currentDepth - 1].pv.line[0])
            << std::endl;
}

void SearchWorker::aspirationWindow() {
  Depth depth = currentDepth;
  Value alpha = -VAL_INFINITE, beta = VAL_INFINITE, delta = 10;

  Result pr = results[currentDepth - 1];
  Result &r = results[currentDepth];

  if (currentDepth >= 4) {
    alpha = std::max(int(-VAL_INFINITE), pr.best - delta);
    beta = std::min(int(VAL_INFINITE), pr.best + delta);
  }

  while (true) {
    r.best = search(rootPos, r.pv, alpha, beta, std::max(int(depth), 1), false);

    if (stop)
      return;

    if (r.best > alpha and r.best < beta) {
      depth = currentDepth;
      break;
    }

    else if (r.best <= alpha) {
      beta = (alpha + beta) / 2;
      alpha = std::max(int(-VAL_INFINITE), r.best - delta);
      depth = currentDepth;
      r.best = pr.best;
      r.pv = pr.pv;
    }

    else if (r.best >= beta) {
      beta = std::min(int(VAL_INFINITE), r.best + delta);
      depth -= (std::abs(r.best) <= VAL_INFINITE / 2);
    }

    delta += delta / 3;
  }
  std::cout << pv(r.pv) << std::endl;
}

void SearchWorker::updateKiller(Move killer, int ply) {
  if (ss.killer[ply][0] != killer) {
    ss.killer[ply][1] = ss.killer[ply][1];
    ss.killer[ply][0] = killer;
  }
}

void SearchWorker::updateHistory(Position &pos, Move history, Depth depth) {
  ss.history[pos.getPiece(history.from())][history.to()] += depth * depth;
}

void SearchWorker::updateCaptureHistory(Position &pos, Move history,
                                        Depth depth) {
  ss.captureHistory[pos.getPiece(history.from())][history.to()]
                   [pos.getPieceType(history.to())] += depth * depth;
}

Value SearchWorker::search(Position &pos, PVLine &parentPV, Value alpha,
                           Value beta, Depth depth, bool cutNode) {

  const bool pvNode = (alpha != beta - 1) and pos.state()->move != Move::null();
  const bool rootNode = (ply == 0);
  const bool inCheck = pos.isInCheck();

  PVLine childPV{};
  bool doFullSearch, skipQuiets, improving, oppWorsening;
  Value value, rAlpha, rBeta, evaluation, bestScore = -VAL_INFINITE,
                                          oldAlpha = alpha, seeMargin[2];
  Value ttHit = 0, ttValue = 0, ttEval = VAL_NONE;
  HashFlag ttFlag = HashNone;
  Depth R, ttDepth = 0, extension, newDepth;
  Count moveCount = 0;
  Move bestMove = Move::none(), move, ttMove = Move::none();
  BoardState st{};
  NodeState &ns = nodeStates[ply];

  ns.dExtensions = nodeStates[ply - 1].dExtensions;

  if (depth <= 0)
    return quiescence(pos, parentPV, alpha, beta); // Return quiescence

  // Reset killer moves
  ss.killer[ply + 1][0] = ss.killer[ply + 1][1] = Move::none();
  nodeStates[ply + 1].excluded = bestMove = Move::none();

  // Generate moves to check for draws
  if ((nodes & 2047) == 0)
    checkTime();

  // Increment nodes
  nodes++;

  selDepth = rootNode ? 0 : std::max(+selDepth, ply + 1);

  parentPV.length = 0;

  depth = std::max(depth, Depth(0));

  // Early exit conditions
  if (!rootNode) {
    // From Ethreal Chess Engine (add variance to draw score to avoid
    // blindness to 3 fold lines)
    if (pos.isDraw(ply)) {
      return 1 - (nodes & 2);
    }
    // Check if reached max search depth
    if (ply >= MAX_DEPTH - 1) {
      return inCheck ? 0 : eval(pos);
    }

    rAlpha = std::max(int(alpha), -VAL_MATE + ply);
    rBeta = std::min(int(beta), VAL_MATE - ply - 1);

    if (rAlpha >= rBeta)
      return rAlpha;
  }

  if (ns.excluded == Move::none() and
      (ttHit = TTProbe(pos.state()->key, ply, ttMove, ttValue, ttEval, ttDepth,
                       ttFlag))) {
    if (ttDepth >= depth and ttMove and ttValue >= beta and
        (ttFlag == HashBeta)) {
      if (!pos.isCapture(ttMove)) {
        updateHistory(pos, ttMove, depth);
        updateKiller(ttMove, ply);
      } else
        updateCaptureHistory(pos, ttMove, depth);
    }
    // Only cut with a greater depth search, and do not return
    // when in a PvNode, unless we would otherwise hit a qsearch
    if (ttDepth >= depth and (depth == 0 || !pvNode) and
        (cutNode || ttValue <= alpha)) {
      if (ttFlag == HashExact || (ttFlag == HashBeta and ttValue >= beta) ||
          (ttFlag == HashAlpha and ttValue <= alpha)) {
        return ttValue;
      }
    }

    if (!pvNode and ttDepth >= depth - 1 and (ttFlag & HashAlpha) and
        (cutNode || ttValue <= alpha) and ttValue + 200 <= alpha and
        alpha <= VAL_MATE_BOUND)
      return alpha;
  }

  // Static eval
  evaluation = ns.staticEval = inCheck              ? VAL_NONE
                               : ttEval != VAL_NONE ? ttEval
                                                    : eval(pos);

  // Improving
  improving = !inCheck and evaluation > results[currentDepth - 2].best;

  // Opponent worsening
  oppWorsening = ns.staticEval + nodeStates[ply - 1].staticEval > 2;

  seeMargin[0] = -20 * depth * depth;
  seeMargin[1] = -203 * depth;

  if (!ttHit and !inCheck and ns.excluded == Move::none()) {
    TTStore(pos.state()->key, ply, Move::none(), VAL_NONE, evaluation, Value(0),
            HashNone);
  }

  if (!pvNode and !inCheck and depth <= 8 and ns.excluded == Move::none() and
      evaluation - 65 * std::max(0, (depth - improving)) >= beta)
    return evaluation;

  if (!pvNode and !inCheck and ns.excluded == Move::none() and depth <= 4 and
      evaluation + 3488 <= alpha)
    return evaluation;

  // Futility pruning
  if (!pvNode && depth < 12 &&
      evaluation -
              futility_margin(depth, cutNode and !ttHit, improving,
                              oppWorsening) -
              nodeStates[ply - 1].staticEval / 267 >=
          beta and
      evaluation >= beta and evaluation < VAL_MATE_BOUND and
      (ttMove == Move::none() or pos.isCapture(ttMove))) {
    return beta > -VAL_MATE_BOUND ? (evaluation + beta) / 2 : evaluation;
  }

  // Null move pruning
  if (!pvNode and !rootNode and !inCheck and evaluation >= beta and
      depth >= 3 and pos.getNonPawnMaterial(pos.getSideToMove()) > 0 and
      pos.state()->move != Move::null() and ns.excluded == Move::none() and
      (!ttHit || !(ttFlag & HashAlpha) || ttValue >= beta)) {

    pos.makeNullMove(st);

    R = 4 + depth / 5 + std::min(3, (evaluation - beta) / 200);

    ply++;

    Value nullValue =
        -search(pos, childPV, -beta, -beta + 1, depth - R, !cutNode);

    ply--;

    pos.unmakeNullMove();

    if (stop)
      return 0;

    if (nullValue >= beta and nullValue < VAL_MATE_BOUND)
      return nullValue;
  }

  // If we are in a pv node but there is no table move, then the move is
  // likely bad
  if (!rootNode and pvNode and !cutNode and ttMove == Move::none() and ttHit)
    depth -= 1;

  // Check if the depth is 0, if so return the quiescence search
  if (depth <= 0)
    return quiescence(pos, parentPV, alpha, beta); // Return quiescence

  // Internal iterative deepening
  if (cutNode and depth >= 8 and ttMove == Move::none())
    depth -= 1;

  rBeta = beta + 170 - 64 * improving;
  if (!pvNode and depth > 3 and std::abs(beta) < VAL_MATE_BOUND and
      !(ttDepth >= depth - 3 and ttValue != VAL_NONE and ttValue < rBeta)) {

    MovePicker mp(pos, ss, rBeta - evaluation, ttMove);
    while ((move = mp.pickNextMove(true)) != Move::none()) {
      if (move == ns.excluded)
        continue;

      pos.makeMove(move, st);

      ply++;

      value = -quiescence(pos, parentPV, -rBeta, -rBeta + 1);

      if (value > rBeta)
        value = -search(pos, childPV, -rBeta, -rBeta + 1,
                        std::max(0, depth - 4), !cutNode);

      ply--;

      pos.unmakeMove();

      if (stop)
        return 0;

      if (value >= rBeta) {
        TTStore(pos.state()->key, ply, move, value, evaluation, depth - 3,
                HashBeta);
        return std::abs(value) < VAL_MATE_BOUND ? value - (rBeta - beta)
                                                : value;
      }
    }
  }

  Square prevSq = (pos.state()->move).isOk() ? (pos.state()->move).to() : NO_SQ;

  bool ttCapture = ttMove.isOk() and pos.isCapture(ttMove);

  // Sort moves
  MovePicker mp(pos, ss, ply, depth, ttMove);

  skipQuiets = false;

  while ((move = mp.pickNextMove(false)) != Move::none()) {
    if (move == ns.excluded)
      continue;

    bool isCapture = pos.isCapture(move);
    bool givesCheck = pos.givesCheck(move);

    if (skipQuiets and !(isCapture or givesCheck or inCheck))
      continue;

    R = LMRTable[std::min(63, int(depth))][std::min(63, int(moveCount))];

    int hist = isCapture
                   ? ss.captureHistory[pos.getPiece(move.from())][move.to()]
                                      [pos.getPieceType(move.to())]
                   : ss.history[pos.getPiece(move.from())][move.to()];

    if (!rootNode and pos.getNonPawnMaterial(pos.getSideToMove()) > 0 and
        bestScore >= -VAL_MATE_BOUND and depth <= 8 and
        moveCount >= LateMovePruningCounts[improving][depth])
      skipQuiets = true;

    if (!isCapture and bestScore > -VAL_MATE_BOUND) {

      int lmrDepth = std::max(0, newDepth - R);
      Value fmpMargin = 77 + 52 * lmrDepth;

      if (!inCheck and evaluation + fmpMargin <= alpha and lmrDepth <= 8 and
          hist < (improving ? 900 : 375))
        skipQuiets = true;

      if (!inCheck and lmrDepth <= 8 and evaluation + fmpMargin + 165 <= alpha)
        skipQuiets = true;
    }

    if (bestScore > -VAL_MATE_BOUND and depth <= 10 and
        mp.genStage > GOOD_CAPTURE and !pos.SEE(move, seeMargin[isCapture]))
      continue;

    // Increment legal move counter
    moveCount++;

    extension = 0;

    if (ply < currentDepth * 2) {

      if (depth >= 4 + pvNode and ns.excluded == Move::none() and
          move == ttMove and !rootNode and ttDepth >= depth - 3 and
          std::abs(ttValue) < VAL_MATE_BOUND and (ttFlag & HashBeta)) {

        Value singularBeta = std::max(ttValue - 58 * depth / 64, -VAL_MATE);

        parentPV.length = 0;

        ns.excluded = move;
        value = -search(pos, parentPV, singularBeta - 1, singularBeta,
                        (depth - 1) / 2, cutNode);
        ns.excluded = Move::none();

        bool double_extend = (!pvNode and ns.dExtensions <= 16) and
                             (pvNode and !ttCapture and ns.dExtensions <= 5 and
                              value < singularBeta - 37);

        if (singularBeta >= beta)
          return singularBeta;
        else
          extension = double_extend          ? 2
                      : value < singularBeta ? 1
                      : ttValue >= beta      ? -1
                      : cutNode              ? -1
                      : ttValue <= value     ? -1
                                             : 0;
      } else
        extension = inCheck;
    }

    newDepth = depth + extension;

    ns.dExtensions = nodeStates[ply - 1].dExtensions + (extension >= 2);

    // Make move
    pos.makeMove(move, st);

    // Increment ply
    ply++;

    if (depth >= 2 and moveCount > 1 + rootNode) {

      if (!isCapture) {

        R += !pvNode + !improving;

        R += inCheck and pos.getPieceType(move.to()) == KING;

        R -= mp.genStage < INIT_QUIET;

        // R += std::min(2, std::abs(evaluation - alpha) / 350);

        // R -= ss.history[pos.getPiece(move.from())][move.to()] / 350;
      } else {
        R = 3 - (ss.captureHistory[pos.getPiece(move.from())][move.to()]
                                  [pos.getPieceType(move.to())] /
                 300);
        R -= inCheck;
      }

      R = std::min(depth - 1, std::max(+R, 1));

      value = -search(pos, childPV, -alpha - 1, -alpha, newDepth - R, true);

      doFullSearch = value > alpha and R != 1;
    } else
      doFullSearch = !pvNode || moveCount > 1;

    if (doFullSearch)
      value = -search(pos, childPV, -alpha - 1, -alpha, newDepth - 1 - (R > 3),
                      !cutNode);

    if (pvNode and (moveCount == 1 || value > alpha))
      value = -search(pos, childPV, -beta, -alpha, newDepth - 1, false);

    // Decrement ply
    ply--;
    // Unmake move
    pos.unmakeMove();

    if (stop)
      return 0;

    if (value > bestScore) {
      bestScore = value;

      if (value > alpha) {

        bestMove = move;
        alpha = value;

        updatePV(parentPV, childPV, move);

        if (value >= beta)
          break;
      }
    }
  }

  if (bestScore >= beta and !pos.isCapture(bestMove)) {
    updateKiller(bestMove, ply);
    updateHistory(pos, bestMove, depth);
  }

  if (bestScore >= beta) {
    updateCaptureHistory(pos, bestMove, depth);
  }

  // Check if there are no moves, if so return the evaluation
  if (moveCount == 0) {
    // Check if in check
    return ns.excluded != Move::none() ? alpha : inCheck ? -VAL_MATE + ply : 0;
  }

  if (ns.excluded == Move::none()) {
    ttFlag = bestScore >= beta      ? HashBeta
             : bestScore > oldAlpha ? HashExact
                                    : HashAlpha;
    bestMove = ttFlag == HashAlpha ? Move::none() : bestMove;
    TTStore(pos.state()->key, ply, bestMove, bestScore, evaluation, depth,
            ttFlag);
  }

  return bestScore;
}

Value SearchWorker::quiescence(Position &pos, PVLine &parentPV, Value alpha,
                               Value beta) {

  PVLine childPV{};

  Count moveCount = 0;
  Move move, ttMove = Move::none(), bestMove = Move::none();
  Value evaluation = 0, ttValue = 0, ttEval = VAL_NONE, value = 0,
        bestScore = -VAL_INFINITE, oldAlpha = alpha, futilityBase,
        futilityValue;
  bool ttHit;
  Depth ttDepth = 0;
  HashFlag ttFlag = HashNone;
  BoardState st{};

  const bool pvNode = (alpha != beta - 1);
  const bool inCheck = pos.isInCheck();

  parentPV.length = 0;

  selDepth = std::max(+selDepth, ply + 1);

  // Increment nodes
  nodes++;

  // Generate moves to check for draws
  if ((nodes & 2047) == 0)
    checkTime();

  // From Ethreal Chess Engine (add variance to draw score to avoid
  // blindness to 3 fold lines)
  if (pos.isDraw(ply)) {
    return 1 - (nodes & 2);
  }

  // Check if reached max search depth
  if (ply >= MAX_DEPTH - 1) {
    return eval(pos);
  }

  // Probe transposition table
  if ((ttHit = TTProbe(pos.state()->key, ply, ttMove, ttValue, ttEval, ttDepth,
                       ttFlag))) {
    // Table is exact or produces a cutoff
    if (!pvNode and
        (ttFlag == HashExact || (ttFlag == HashBeta and ttValue >= beta) ||
         (ttFlag == HashAlpha and ttValue <= alpha)) and
        pos.state()->fiftyMove < 90)
      return ttValue;
  }

  if (inCheck)
    bestScore = futilityBase = -VAL_INFINITE;
  else {
    if (ttHit) {
      evaluation = bestScore = ttEval == VAL_NONE ? eval(pos) : ttEval;

      if (ttValue != VAL_NONE and
          (ttFlag & (ttValue > bestScore ? HashBeta : HashAlpha))) {
        bestScore = ttValue;
      }
    } else {
      evaluation = bestScore = eval(pos);
    }

    if (bestScore >= beta) {

      if (!ttHit)
        TTStore(pos.state()->key, ply, Move::none(), bestScore, evaluation, 0,
                HashBeta);

      return bestScore;
    }

    if (bestScore > alpha)
      alpha = bestScore;

    futilityBase = evaluation + 250;
  }

  Square prevSq = (pos.state()->move).isOk() ? (pos.state()->move).to() : NO_SQ;

  // Sort moves
  MovePicker mp(pos, ss, ply, 0, Move::none(), true,
                std::max(1, alpha - evaluation - 123));

  while ((move = mp.pickNextMove(true)) != Move::none()) {

    bool givesCheck = pos.givesCheck(move);

    if (bestScore > -VAL_MATE_BOUND and
        pos.getNonPawnMaterial(pos.getSideToMove()) > 0) {
      if (!givesCheck and !move.isPromotion() and move.to() != prevSq and
          futilityBase > -VAL_MATE_BOUND) {
        if (moveCount > 2)
          continue;

        futilityValue = futilityBase + PieceValue[pos.getPiece(move.to())];

        if (futilityValue <= alpha) {
          bestScore = std::max(bestScore, futilityValue);
          continue;
        }

        if (futilityBase <= alpha and !pos.SEE(move, 1)) {
          bestScore = std::max(bestScore, futilityBase);
          continue;
        }

        if (futilityBase > alpha and
            !pos.SEE(move, (alpha - futilityBase) * 4)) {
          bestScore = alpha;
          continue;
        }
      }

      if (!pos.SEE(move, -78))
        continue;
    }

    // Make move
    pos.makeMove(move, st);
    // Increment ply
    ply++;
    // Move count increment
    moveCount++;
    // Full search
    value = -quiescence(pos, childPV, -beta, -alpha);
    // Decrement ply
    ply--;
    // Unmake move
    pos.unmakeMove();

    if (stop)
      return 0;

    if (value > bestScore) {
      bestScore = value;
      bestMove = move;
      if (value > alpha) {
        // Update alpha
        alpha = value;

        updatePV(parentPV, childPV, move);

        if (value >= beta) {
          return beta;
        }
      }
    }
  }

  if (inCheck and bestScore == -VAL_INFINITE) {
    return -VAL_MATE + ply;
  }

  ttFlag = bestScore >= beta      ? HashBeta
           : bestScore > oldAlpha ? HashExact
                                  : HashAlpha;
  TTStore(pos.state()->key, ply, bestMove, bestScore, evaluation, 0, ttFlag);

  return bestScore;
}
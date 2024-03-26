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
  rootPos = pos;
  rootState = *pos.state();
  nodes = bestMoveChanges = 0;
  tm = time;
  maxDepth = depth;
  pvStability = failHigh = failHighFirst = 0;

  std::fill(pvLine, pvLine + MAX_DEPTH + 1, PVLine{});

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
      bestValue[currentDepth] - bestValue[currentDepth - 3];

  const double scoreFactor = std::max(0.75, std::max(1.25, scoreChange * 0.05));

  return elapsed > tm.idealStopTime * scoreFactor * pvFactor * 0.5;
}

void SearchWorker::tmUpdate() {
  pvStability =
      (pvLine[currentDepth].line[0] == pvLine[currentDepth - 1].line[0])
          ? std::min(10, pvStability + 1)
          : 0;
}

std::string SearchWorker::pv(PVLine pvLine) {
  std::stringstream ss;

  Time time = tm.elapsed() + 1;
  int hashFull = TTHashFull();

  Value score = bestValue[currentDepth];

  ss << "info"
     << " depth " << currentDepth << " score " << score2Str(score) << " nodes "
     << nodes << " nps " << nodes * 1000 / time << " hashfull " << hashFull
     << " tbhits " << 0 << " time " << time << " pv";

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

void SearchWorker::searchPosition() {
  // Iterative deepening
  TTUpdate();
  iterativeDeepening();
  searching = false;
  waitForSearchFinished();
}

void SearchWorker::iterativeDeepening() {

  std::fill(bestValue, bestValue + MAX_DEPTH + 1, -VAL_INFINITE);

  for (currentDepth = 1; currentDepth <= maxDepth; currentDepth++) {
    aspirationWindow();

    if (stop) {
      break;
    }

    tmUpdate();
    std::cout << pv(pvLine[currentDepth]) << std::endl;
    std::cout << "Best move changes: " << bestMoveChanges << std::endl;
    if (failHigh)
      std::cout << "Move ordering score: " << failHighFirst * 100 / failHigh
                << std::endl;
  }
  std::cout << "bestmove " << move2Str(pvLine[currentDepth - 1].line[0])
            << std::endl;
}

void SearchWorker::aspirationWindow() {
  Depth depth = currentDepth;
  Value alpha = -VAL_INFINITE, beta = VAL_INFINITE, delta = 10;

  Value best;
  PVLine current{};

  if (currentDepth >= 4) {
    alpha = std::max(int(-VAL_INFINITE), bestValue[currentDepth - 1] - delta);
    beta = std::min(int(VAL_INFINITE), bestValue[currentDepth - 1] + delta);
  }

  while (true) {
    best =
        search(rootPos, current, alpha, beta, std::max(int(depth), 1), false);

    if (stop) {
      break;
    }

    if (best > alpha and best < beta) {
      pvLine[currentDepth] = current;
      bestValue[currentDepth] = best;
      depth = currentDepth;
      break;
    }

    else if (best <= alpha) {
      beta = (alpha + beta) / 2;
      alpha = std::max(int(-VAL_INFINITE), alpha - delta);
      depth = currentDepth;
    }

    else if (best >= beta) {
      beta = std::min(int(VAL_INFINITE), beta + delta);
      depth -= (std::abs(best) <= VAL_INFINITE / 2);
      pvLine[currentDepth] = current;
      bestValue[currentDepth] = best;
    }

    delta += delta / 2;
  }
}

void SearchWorker::updateKiller(Move killer, Depth depth) {
  if (ss.killer[depth][0] != killer) {
    ss.killer[depth][1] = ss.killer[depth][1];
    ss.killer[depth][0] = killer;
  }
}

void SearchWorker::updateHistory(Position &pos, Move history, Depth depth) {
  ss.history[pos.getPiece(history.from())][history.to()] += depth * depth;
}

Value SearchWorker::search(Position &pos, PVLine &parentPV, Value alpha,
                           Value beta, Depth depth, bool cutNode) {

  const bool pvNode = (alpha != beta - 1);
  const bool rootNode = (ply == 0);
  const bool inCheck = pos.isInCheck();

  PVLine childPV{};
  bool doFullSearch = false, skipQuiets = false, improving = false;
  Value value, rAlpha, rBeta, evaluation, bestScore = -VAL_INFINITE,
                                          oldAlpha = alpha, probCutBeta;
  Value ttHit = 0, ttValue = 0, ttEval = VAL_NONE;
  HashFlag ttFlag = HashNone;
  Depth R, ttDepth = 0, extension, newDepth;
  Count moveCount = 0;
  Move bestMove = Move::none(), move, ttMove = Move::none();
  BoardState st{};

  // Generate moves to check for draws
  if ((nodes & 2047) == 0)
    checkTime();

  // Check if the depth is 0, if so return the quiescence search
  if (depth <= 0)
    return quiescence(pos, parentPV, alpha, beta); // Return quiescence

  // Increment nodes
  nodes++;

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

  if ((ttHit = TTProbe(pos.state()->key, ply, ttMove, ttValue, ttEval, ttDepth,
                       ttFlag))) {

    if (ttDepth >= depth and ttValue != VAL_NONE and ttMove and
        ttValue >= beta and (ttFlag == HashBeta)) {
      if (!pos.isCapture(ttMove)) {
        updateHistory(pos, ttMove, depth);
        updateKiller(ttMove, depth);
      }
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
        (cutNode || ttValue <= alpha) and ttValue + 200 <= alpha)
      return alpha;
  }

  // Static eval
  evaluation = inCheck ? VAL_NONE : ttEval != VAL_NONE ? ttEval : eval(pos);

  // Improving
  improving = !inCheck and evaluation > bestValue[currentDepth - 2];

  rBeta = std::min(beta + 100, VAL_MATE_BOUND - 1);

  if (!ttHit and !inCheck) {
    TTStore(pos.state()->key, ply, Move::none(), VAL_NONE, evaluation, Value(0),
            HashNone);
  }

  if (!pvNode and !inCheck and depth <= 8 and
      evaluation - 65 * std::max(0, (depth - improving)) >= beta)
    return evaluation;

  if (!pvNode and !inCheck and depth <= 4 and evaluation + 3488 <= alpha)
    return evaluation;

  // static null move pruning
  if (depth < 3 && !rootNode && !pvNode && !inCheck &&
      abs(beta - 1) > -VAL_INFINITE + 100) {
    int eval_margin = 120 * depth;

    if (evaluation - eval_margin >= beta) {
      return evaluation - eval_margin;
    }
  }

  // Null move pruning
  if (!rootNode and !inCheck and evaluation >= beta and depth >= 3 and
      pos.getNonPawnMaterial(pos.getSideToMove()) > 0 and
      pos.state()->move != Move::null() and
      (!ttHit || !(ttFlag & HashAlpha) || ttValue >= beta)) {

    pos.makeNullMove(st);

    R = 4 + depth / 5 + std::min(2, (evaluation - beta) / 200);

    ply++;

    Value nullValue =
        -search(pos, childPV, -beta, -beta + 1, depth - R, !cutNode);

    ply--;

    pos.unmakeNullMove();

    if (stop) {
      return 0;
    }

    if (nullValue >= beta)
      return nullValue > VAL_MATE_BOUND ? beta : nullValue;
  }

  // Razoring
  if (!pvNode && depth < 3 && !inCheck) {
    value = evaluation + 125;

    Value newValue;

    if (value < beta) {
      if (depth == 1) {
        newValue = quiescence(pos, parentPV, alpha, beta);

        return (newValue > value) ? newValue : value;
      }

      value += 175;

      if (value < beta && depth <= 2) {
        newValue = quiescence(pos, parentPV, alpha, beta);

        if (newValue < beta)
          return (newValue > value) ? newValue : value;
      }
    }
  }

  // Futility pruning
  if (!pvNode && depth < 11 && !inCheck &&
      evaluation - (120 - 40 * !pvNode) * depth >= beta) {
    return beta > -VAL_MATE ? (evaluation + beta) / 2 : evaluation;
  }

  // Internal iterative deepening
  if (cutNode and depth >= 7 and ttMove == Move::none()) {
    depth -= 1;
  }

  // Prob cut pruning
  probCutBeta = beta + 170 - 64 * improving;

  if (!pvNode and depth > 3 and std::abs(beta) < VAL_MATE_BOUND and
      !(ttDepth >= depth - 3 and ttValue != VAL_NONE and
        ttValue < probCutBeta)) {
    MovePicker mp(pos, ss, probCutBeta - evaluation, ttMove);

    while ((move = mp.pickNextMove(true)) != Move::none()) {
      if (!pos.isLegal(move))
        continue;

      pos.makeMove(move, st);

      ply++;

      value = -quiescence(pos, childPV, -probCutBeta, -probCutBeta + 1);

      if (value >= probCutBeta) {
        value = -search(pos, childPV, -probCutBeta, -probCutBeta + 1, depth - 4,
                        !cutNode);
      }

      ply--;

      pos.unmakeMove();

      if (value >= probCutBeta) {
        TTStore(pos.state()->key, ply, move, value, evaluation, depth - 3,
                HashBeta);

        return std::abs(value) < VAL_MATE_BOUND ? value - (probCutBeta - beta)
                                                : value;
      }

      if (stop)
        return 0;

      if (value >= probCutBeta)
        return value;
    }
  }

  // Sort moves
  MovePicker mp(pos, ss, ply, depth, ttMove);

  while ((move = mp.pickNextMove(skipQuiets)) != Move::none()) {
    if (!pos.isLegal(move))
      continue;

    bool isCapture = pos.isCapture(move);

    R = 0.5 + std::log(depth) * std::log(moveCount) / 2.5;

    newDepth = depth + inCheck;

    // Increment legal move counter
    moveCount++;

    if (!rootNode and pos.getNonPawnMaterial(pos.getSideToMove()) > 0 and
        bestScore >= -VAL_MATE_BOUND) {

      if (!skipQuiets)
        skipQuiets = moveCount >=
                     (improving ? 8 + depth * depth : (8 + depth * depth) / 2);

      if (isCapture)
        if (!pos.SEE(move, -203 * depth))
          continue;
        else if (!pos.SEE(move, -27 * (newDepth - R) * (newDepth - R)))
          continue;
    }

    // Make move
    pos.makeMove(move, st);

    // Increment ply
    ply++;

    if (!rootNode and depth >= 2 and moveCount >= 4 + rootNode and
        !isCapture and !move.isPromotion() and !inCheck) {

      R += !pvNode + !improving;

      R += inCheck and pos.getPieceType(move.to()) == KING;

      R -= ss.history[pos.getPiece(move.from())][move.to()] / 350;

      R = std::min(depth - 1, std::max(+R, 1));

      value = -search(pos, childPV, -alpha - 1, -alpha, newDepth - R, true);

      doFullSearch = value > alpha and R != 1;
    } else
      doFullSearch = !pvNode || moveCount > 1;

    if (doFullSearch)
      value = -search(pos, childPV, -alpha - 1, -alpha, newDepth - 1, !cutNode);

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
      bestMove = move;
      if (value > alpha) {
        // Update alpha
        alpha = value;

        updatePV(parentPV, childPV, move);
        if (!pos.isCapture(move))
          updateHistory(pos, bestMove, depth);

        // Update best move changes
        if (rootNode and moveCount > 1) {
          ++bestMoveChanges;
        }

        if (value >= beta) {
          if (moveCount == 1)
            failHighFirst++;
          failHigh++;
          if (!pos.isCapture(move))
            updateKiller(bestMove, depth);
          break;
        }
      }
    }
  }

  // Check if there are no moves, if so return the evaluation
  if (moveCount == 0) {
    // Check if in check
    return inCheck ? -VAL_MATE + ply : 0;
  }

  if (!rootNode) {
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

  // // Get static evaluation
  // evaluation = bestScore = ttEval != VAL_NONE ? ttEval
  //                          : inCheck          ? -VAL_MATE + ply
  //                                             : eval(pos);

  // // Save static evaluation
  // if (!ttHit and !inCheck) {
  //   TTStore(pos.state()->key, ply, Move::none(), VAL_NONE, evaluation, 0,
  //           HashNone);
  // }

  // if (bestScore >= beta || ply >= MAX_DEPTH - 1) {
  //   return bestScore;
  // }

  // if (alpha < bestScore) {
  //   alpha = bestScore;
  // }

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
    if (!pos.isLegal(move))
      continue;

    if (bestScore > -VAL_MATE_BOUND and
        pos.getNonPawnMaterial(pos.getSideToMove()) > 0) {
      if (!move.isPromotion() and move.to() != prevSq and
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
    // Increment move count
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
          break;
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
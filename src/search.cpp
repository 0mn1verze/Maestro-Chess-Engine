#include <iostream>

#include "eval.hpp"
#include "history.hpp"
#include "movepicker.hpp"
#include "search.hpp"
#include "uci.hpp"

namespace Maestro {

// Initialise the time manager
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
    optimumTime = 1.80 * (time - MOVE_OVERHEAD) / mtg + inc;
    maximumTime = 10.00 * (time - MOVE_OVERHEAD) / mtg + inc;
  } else {
    // X + Y time management
    optimumTime = 2.50 * (time - MOVE_OVERHEAD + 25 * inc) / 50;
    maximumTime = 10.00 * (time - MOVE_OVERHEAD + 25 * inc) / 50;
  }

  // Cap time allocation using move overhead
  optimumTime = std::min(optimumTime, time - MOVE_OVERHEAD);
  maximumTime = std::min(maximumTime, time - MOVE_OVERHEAD);

  std::cout << "Optimum: " << optimumTime << std::endl;
  std::cout << "Maximum: " << maximumTime << std::endl;
}

// Update the total search time and check if its exceeded
bool SearchWorker::checkTM(Depth &lastBestMoveDepth, int &pvStability,
                           int &bestValue) const {
  if (_completedDepth <= 4)
    return false;

  bool stable = lastBestMoveDepth + 3 <= _completedDepth;

  pvStability = stable ? std::min(10, pvStability + 1) : 0;

  // Scale time between 80% && 120% , based on stability of best moves
  const double pvFactor = 1.2 - 0.04 * pvStability;

  // Scale time between 80% && 120% , based on score fluctuation
  const double scoreFluctuation = abs(bestPreviousAvgScore - bestValue);
  const double scoreFactor = 1.20 - 0.04 * std::min(scoreFluctuation, 10.00);

  // Scale time between 50% && 240%, based on what % of nodes are used for
  // searching the best move
  const U64 nodesUsedForBestMove = rootMoves[0].effort;
  const double nodesBestPercent =
      nodesUsedForBestMove / threads.nodesSearched();
  const double nodesFactor = std::max(0.5, 2 * nodesBestPercent + 0.4);

  double totalTime = tm.optimum() * pvFactor * scoreFactor * nodesFactor;

  auto elapsed = tm.elapsed();

  return elapsed >= totalTime;
}

// Check if we should stop the search
void SearchWorker::checkTime() const {
  if (isMainThread() && _completedDepth >= 4 &&
      ((limits.isUsingTM() && tm.elapsed() >= tm.maximum()) ||
       (limits.movetime && tm.elapsed() >= limits.movetime)))
    threads.stop = threads.abortedSearch = true;
}

void SearchWorker::getPV(SearchWorker &best, Depth depth) const {
  auto &rootMoves = best.rootMoves;
  auto &pos = best.rootPos;

  const U64 nodes = threads.nodesSearched();

  bool updated = rootMoves[0].score != -VAL_INFINITE;

  Depth d = updated ? depth : std::max(1, depth - 1);
  Value v = updated ? rootMoves[0].score : rootMoves[0].prevScore;

  if (v == -VAL_INFINITE)
    v = VAL_ZERO;

  bool isExact = !updated;

  std::string pv;

  for (Move m : rootMoves[0].pv)
    pv += move2Str(m) + " ";

  if (!pv.empty())
    pv.pop_back();

  PrintInfo info;

  info.depth = d;
  info.score = v;
  info.selDepth = rootMoves[0].selDepth;
  info.timeMs = tm.elapsed() + 1;
  info.nodes = nodes;
  info.nps = nodes * 1000 / info.timeMs;
  info.pv = pv;
  info.hashFull = tt.hashFull();

  UCI::uciReport(info);
}

void SearchWorker::updatePV(Move *pv, Move best, const Move *childPV) const {

  for (*pv++ = best; childPV && *childPV != Move::none();)
    *pv++ = *childPV++;

  *pv = Move::none();
}

void SearchWorker::clear() {
  kt.clear();
  ht.clear();
  cht.clear();
  ct.clear();
}

void SearchWorker::startSearch() {

  if (!isMainThread()) {
    iterativeDeepening();
    return;
  }

  tm.init(limits, rootPos.sideToMove(), rootPos.gamePlies());

  tt.newSearch();

  if (rootMoves.empty()) {
    rootMoves.emplace_back(Move::none());
  } else {
    threads.startSearch();
    iterativeDeepening();
  }

  while (!threads.stop && limits.infinite) {
  }

  threads.stop = true;

  threads.waitForThreads();

  SearchWorker *best = this;

  // If we are using time control (no specified depth), and we have a PV, then
  // get the best threads out of the thread pool
  if (!limits.depth && rootMoves[0].pv[0] != Move::none())
    best = threads.getBestThread()->worker.get();

  // If we have a different best thread, print the PV of the best thread
  if (best != this)
    getPV(*best, best->completedDepth());

  auto bestMove = move2Str(best->rootMoves[0].pv[0]);
  std::cout << "bestmove " << bestMove << std::endl;
}

void SearchWorker::iterativeDeepening() {
  Move pv[MAX_PLY + 1];

  Depth lastBestMoveDepth = 0;
  Value lastBestMoveScore = -VAL_INFINITE;
  auto lastBestPV = std::vector<Move>{Move::none()};

  Value bestValue = -VAL_INFINITE;
  Colour us = rootPos.sideToMove();
  int pvStability = 0;
  int delta = 0;

  ttCutOff = 0;

  // Allocate search stack
  constexpr int EXTENSION = 4;
  SearchStack stack[MAX_PLY + EXTENSION + 1] = {};
  SearchStack *ss = stack + EXTENSION;

  for (int i = EXTENSION; i > 0; i--) {
    (ss - i)->ch = &ct.table[0][0][NO_PIECE][A1];
    (ss - i)->staticEval = VAL_NONE;
  }

  for (int i = 0; i <= MAX_PLY; i++)
    (ss + i)->ply = i;

  ss->pv = pv;

  while (++rootDepth < MAX_PLY && !threads.stop &&
         !(limits.depth && isMainThread() && rootDepth > limits.depth)) {
    // Save previous iteration's scores
    for (RootMove &rm : rootMoves)
      rm.prevScore = rm.score;

    searchPosition(ss, bestValue);

    if (!threads.stop)
      _completedDepth = rootDepth;

    // If mate is unproven, revert to the last best pv && score
    if (threads.abortedSearch && rootMoves[0].score != -VAL_INFINITE &&
        rootMoves[0].score >= VAL_MATE_BOUND) {
      Utility::moveToFront(
          rootMoves, [&lastBestPV = std::as_const(lastBestPV)](const auto &rm) {
            return rm == lastBestPV[0];
          });
      rootMoves[0].pv = lastBestPV;
      rootMoves[0].score = lastBestMoveScore;
      // If the best move has changed, update the last best move
    } else if (rootMoves[0].pv[0] != lastBestPV[0]) {
      lastBestPV = rootMoves[0].pv;
      lastBestMoveScore = rootMoves[0].score;
      lastBestMoveDepth = rootDepth;
    }

    if (!isMainThread())
      continue;

    if (limits.isUsingTM() && !threads.stop && _completedDepth >= 4 &&
        (checkTM(lastBestMoveDepth, pvStability, bestValue) ||
         tm.elapsed() >= tm.maximum()))
      threads.stop = true;
  }
}

void SearchWorker::searchPosition(SearchStack *ss, Value &bestValue) {
  // Reset selDepth
  _selDepth = 0;

  aspirationWindows(ss, bestValue);

  std::stable_sort(rootMoves.begin(), rootMoves.end());

  if (isMainThread() &&
      !(threads.abortedSearch && rootMoves[0].score <= VAL_MATE_BOUND))
    getPV(*this, rootDepth);
}

void SearchWorker::aspirationWindows(SearchStack *ss, Value &bestValue) {
  Value alpha = -VAL_INFINITE, beta = VAL_INFINITE, delta = 10;

  Depth depth = rootDepth;

  if (depth >= 4) {
    alpha = std::max(rootMoves[0].score - delta, -VAL_INFINITE);
    beta = std::min(rootMoves[0].score + delta, VAL_INFINITE);
  }

  while (true) {
    bestValue =
        search<ROOT>(rootPos, ss, std::max(1, depth), alpha, beta, false);

    std::stable_sort(rootMoves.begin(), rootMoves.end());

    // If search failed low, adjust window and reset depth
    if (bestValue <= alpha) {
      beta = (alpha + beta) / 2;
      alpha = std::max(alpha - delta, -VAL_INFINITE);
      depth = rootDepth;
    } else if (bestValue >= beta) {
      beta = std::min(beta + delta, VAL_INFINITE);
      depth -= (abs(bestValue) <= VAL_MATE_BOUND / 2);
    } else
      break;

    delta += delta / 3;
  }
}

Value SearchWorker::evaluate(Position &pos) { return Eval::evaluate(pos); }

template <NodeType nodeType>
Value SearchWorker::search(Position &pos, SearchStack *ss, Depth depth,
                           Value alpha, Value beta, bool cutNode) {
  // Initialize variables
  constexpr bool pvNode = nodeType != NON_PV;
  constexpr bool rootNode = nodeType == ROOT;
  const bool allNode = !(pvNode || cutNode);

  // Quiescence search
  if (depth <= 0)
    return qSearch < pvNode ? PV : NON_PV > (pos, ss, alpha, beta);

  // Update node count
  nodes.fetch_add(1, std::memory_order_relaxed);

  // Limit search depth to prevent search explosion
  depth = std::min(depth, MAX_PLY - 1);

  // State variables
  Move pv[MAX_PLY + 1];
  BoardState st;

  Key hashKey;
  Move move, bestMove, excludedMove;
  Value bestValue, value, probCutBeta, hist, seeMargin[2];
  int moveCount;
  bool ttCapture, isCapture, givesCheck, improving, oppWorsening;
  Depth R, newDepth, extensions;

  MoveArray captures, quiets;

  // Initialise node
  Colour us = pos.sideToMove();
  excludedMove = ss->excludedMove;
  ss->moveCount = moveCount = 0;
  ss->inCheck = pos.isInCheck();
  bestValue = -VAL_INFINITE;
  bestMove = Move::none();
  Square prevSq = (ss - 1)->currentMove ? (ss - 1)->currentMove.to() : NO_SQ;

  // Check for remaining time
  if (isMainThread())
    checkTime();

  kt.probe(ss->ply + 1, 0) = Move::none();
  kt.probe(ss->ply + 1, 1) = Move::none();

  // Update seldepth to sent to GUI
  if (pvNode)
    _selDepth = std::max(_selDepth, ss->ply + 1);

  // If we are not in the root node, check for draws and apply mate distance
  // pruning
  if (!rootNode) {

    if (pos.isDraw(ss->ply) || threads.stop.load(std::memory_order_relaxed) ||
        ss->ply >= MAX_PLY)
      return (ss->ply >= MAX_PLY && !ss->inCheck) ? evaluate(pos)
                                                  : valueDraw(nodes);

    // The worst we can do is get mated in the current ply, so the worst score
    // is mated in ply
    alpha = std::max(matedIn(ss->ply), alpha);
    // The best score is mating the opponent in the current ply, so the best
    // score is mate in ply + 1
    beta = std::min(mateIn(ss->ply + 1), beta);

    // If the score returned is better or worse than all the possible scores
    // achievable, then there is no reason to continue searching this node
    if (alpha >= beta)
      return alpha;
  }

  // Transposition Table Lookup
  hashKey = pos.key();
  auto [ttHit, ttData, ttWriter] = tt.probe(hashKey);
  // Processing TT data
  ss->ttHit = ttHit;
  ttData.move = rootNode ? rootMoves[0].pv[0]
                : ttHit  ? ttData.move
                         : Move::none();
  ttData.value =
      ttHit ? TTable::valueFromTT(ttData.value, ss->ply, pos.fiftyMove())
            : VAL_NONE;
  ss->ttPV = excludedMove ? ss->ttPV : pvNode || (ttHit && ttData.isPV);
  ttCapture = ttData.move && pos.isCapture(ttData.move);

  // Hash table move cutoff
  if (!pvNode && !excludedMove &&
      ttData.depth > depth - (ttData.value <= beta) &&
      ttData.value != VAL_NONE &&
      (ttData.flag & (ttData.value >= beta ? FLAG_LOWER : FLAG_UPPER)) &&
      (cutNode == (ttData.value >= beta) || depth > 8)) {

    // If ttMove is quiet update the move sorting heuristics on TT hit
    if (ttData.move && ttData.value >= beta) {
      // Bonus for a quiet ttMove that fails high
      if (!ttCapture)
        ht.update(pos, ttData.move, statBonus(depth));

      // Extra penalty for early quiet moves of the previous ply
      if (prevSq != NO_SQ && (ss - 1)->moveCount <= 2 && !pos.captured())
        updateContinuations(ss - 1, pos, (ss - 1)->currentMove,
                            -statBonus(depth + 1));
    }

    if (pos.fiftyMove() < 90)
      return ttData.value;
  }

  // Static Evaluation
  if (ss->inCheck) {
    ss->staticEval = (ss - 2)->staticEval;
    improving = false;
    goto moves_loop;
  } else if (excludedMove) {

  } else if (ss->ttHit) {
    ss->staticEval = ttData.eval != VAL_NONE ? ttData.eval : evaluate(pos);
  } else {
    ss->staticEval = evaluate(pos);

    ttWriter.write(hashKey, VAL_NONE, ss->ttPV, FLAG_NONE, DEPTH_UNSEARCHED,
                   Move::none(), ss->staticEval, tt._gen);
  }

  improving = ss->staticEval > (ss - 2)->staticEval;

  oppWorsening = ss->staticEval + (ss - 1)->staticEval > 2;

  // Static Exchange Evaluation Pruning Margins
  seeMargin[0] = -20 * depth * depth;
  seeMargin[1] = -64 * depth;

  // Reverse Futility pruning (If eval is well enough, assume the eval will hold
  // above beta or cause a cutoff)
  if (!pvNode && !ss->ttPV && depth <= 8 && !excludedMove &&
      ss->staticEval - 70 * std::max(0, depth - improving) >= beta &&
      (!ttData.move || ttCapture) && beta > -VAL_MATE_BOUND &&
      ss->staticEval < VAL_MATE_BOUND)
    return ss->staticEval;

  // Futility Pruning
  if (!pvNode && !ss->ttPV && depth <= 4 && !excludedMove &&
      ss->staticEval + 3000 <= alpha)
    return ss->staticEval;

  // Null Move Pruning
  if (cutNode && (ss - 1)->currentMove != Move::null() && depth >= 2 &&
      ss->staticEval >= beta && !excludedMove && pos.nonPawnMaterial(us) &&
      beta > VAL_MATE_BOUND) {

    R = std::min((ss->staticEval - beta) / 200, 6) + depth / 3 + 5;

    ss->currentMove = Move::null();
    ss->ch = &ct.table[0][0][NO_PIECE][A1];

    pos.makeNullMove(st);
    // Prefetch the next entry in the TT
    TTable::prefetch(tt.firstEntry(pos.key()));

    Value nullValue =
        -search<NON_PV>(pos, ss + 1, depth - R, -beta, -beta + 1, false);

    pos.unmakeNullMove();

    if (nullValue >= beta && nullValue < VAL_MATE_BOUND)
      return nullValue;
  }

  // Internal iterative deepening
  if (pvNode && !ttData.move)
    depth -= 3;

  // Check for quiescence search
  if (depth <= 0)
    return qSearch<PV>(pos, ss, alpha, beta);

  if (cutNode && depth >= 7 and (!ttData.move || ttData.flag == FLAG_UPPER))
    depth -= 2;

  // Probcut
  // If we have a good enough capture or queen promotion, and a previous search
  // returns a value a lot high than beta, we can safely prune the move
  probCutBeta = beta + 200 - 50 * improving - 30 * oppWorsening;
  if (!pvNode && depth > 3 && std::abs(beta) < VAL_MATE_BOUND &&
      !(ttData.depth >= depth - 3 && ttData.value != VAL_NONE &&
        ttData.value < probCutBeta)) {

    MovePicker mp(pos, ttData.move, cht, probCutBeta - ss->staticEval);
    Piece captured;

    while ((move = mp.next()) != Move::none()) {

      if (move == excludedMove)
        continue;

      ss->currentMove = move;
      ss->ch = &ct.table[ss->inCheck][true][pos.movedPiece(move)][move.to()];

      pos.makeMove(move, st);
      // Prefetch the next entry in the TT
      TTable::prefetch(tt.firstEntry(pos.key()));

      // Perform a preliminary qsearch to verify that the move holds
      value = -qSearch<NON_PV>(pos, ss + 1, -probCutBeta, -probCutBeta + 1);

      // If the qsearch held, perform the regular search
      if (value >= probCutBeta)
        value = -search<NON_PV>(pos, ss + 1, depth - 4, -probCutBeta,
                                -probCutBeta + 1, !cutNode);

      pos.unmakeMove(move);

      if (value >= probCutBeta) {
        cht.update(pos, move, statBonus(depth - 2));

        ttWriter.write(hashKey, TTable::valueToTT(value, ss->ply), ss->ttPV,
                       FLAG_LOWER, depth - 3, move, ss->staticEval, tt._gen);

        return std::abs(value) <= VAL_MATE_BOUND ? value - (probCutBeta - beta)
                                                 : value;
      }
    }
  }

  // Razoring
  // If eval is really really low check with qsearch to see if it can exceed
  // alpha, if it can't there is probably no way for the engine to gain a good
  // enough position, so we prune the branch
  if (ss->staticEval < alpha - 500 - 300 * depth * depth) {
    value = qSearch<NON_PV>(pos, ss, alpha - 1, alpha);

    if (value < alpha && std::abs(value) < VAL_MATE_BOUND)
      return value;
  }

moves_loop:

  Continuation *ch[] = {(ss - 1)->ch, (ss - 2)->ch, (ss - 3)->ch, (ss - 4)->ch};

  MovePicker mp(pos, ttData.move, depth, ss->ply, ht, kt, cht, ch);

  while ((move = mp.next()) != Move::none()) {

    // Skip excluded move
    if (move == excludedMove)
      continue;

    // For the searchmoves option, skip moves not in the list.
    // For the multipv option, skip searched nodes.
    if (rootNode && !std::count(rootMoves.begin(), rootMoves.end(), move))
      continue;

    // Clear stack pvs
    if (pvNode)
      (ss + 1)->pv = nullptr;

    // Output current move to UCI
    if (rootNode && isMainThread() && tm.elapsed() > 2500) {
      UCI::uciReportCurrentMove(depth, move, moveCount);
      UCI::uciReportNodes(threads, tt.hashFull(), tm.elapsed());
    }

    // Set variables
    ss->moveCount = moveCount++;
    // Set move flags
    isCapture = pos.isCapture(move);
    givesCheck = pos.givesCheck(move);
    hist = isCapture ? cht.probe(pos, move) : ht.probe(pos, move);

    if (!rootNode && pos.nonPawnMaterial(pos.sideToMove()) > 0 &&
        bestValue >= -VAL_MATE_BOUND) {
      if (moveCount >= (3 + depth * depth) / (2 - improving))
        mp.skipQuietMoves();

      if (!isCapture) {

        Depth lmrDepth = std::max(0, depth - (2 + (moveCount > 6) * depth / 3));
        Depth fmpMargin = 100 + 50 * lmrDepth;

        // Futility Move Pruning with history. If the score is far below alpha
        // and we don't expect anything from this move, we can skip it
        // (Introduces search instability but has more benefits than
        // drawbacks)
        if (!ss->inCheck && ss->staticEval + fmpMargin <= alpha &&
            lmrDepth <= 8 && hist < (improving ? 10000 : 5000))
          mp.skipQuietMoves();

        // Futility Move Pruning. If the score is far below alpha and we don't
        // expect anything from this move, we can skip it (Introduces search
        // instability but has more benefits than drawbacks)
        if (!ss->inCheck && ss->staticEval + fmpMargin + 200 <= alpha &&
            lmrDepth <= 8)
          mp.skipQuietMoves();

        // Continuation pruning. Prune moves with poor continuation histories as
        // the moves are not likely to be good.
        if (mp.stage() > QUIET_INIT && lmrDepth <= 3 &&
            ss->ch->probe(pos, move) < -2500)
          continue;
      }
    }

    // Static Exchanege Evaluation Pruning. Prune moves that are unlikely to be
    // good
    if (bestValue > -VAL_MATE_BOUND && depth <= 10 &&
        mp.stage() > GOOD_CAPTURE &&
        !pos.SEE(move, seeMargin[!isCapture] - hist / 100))
      continue;

    extensions = 0;

    newDepth = depth - 1;

    // Singular extension search, if a move seems a lot better than other moves
    // (Only move), then verify it and extend the search accordingly
    if (ss->ply < rootDepth * 2) {
      if (depth >= 4 && !excludedMove && move == ttData.move && !rootNode &&
          ttData.depth >= depth - 3 &&
          std::abs(ttData.value) < VAL_MATE_BOUND &&
          (ttData.flag & FLAG_LOWER)) {
        Value singularBeta = std::max(ttData.value - depth, -VAL_MATE);
        Depth singularDepth = newDepth / 2;

        ss->excludedMove = move;
        value = search<NON_PV>(pos, ss, singularDepth, singularBeta - 1,
                               singularBeta, cutNode);
        ss->excludedMove = Move::none();

        if (value >= beta)
          return value;

        extensions = (value < singularBeta - 50)   ? 3
                     : (value < singularBeta - 25) ? 2
                     : value < singularBeta        ? 1
                     : ttData.value >= beta        ? -3
                     : cutNode                     ? -2
                     : ttData.value <= value       ? -1
                                                   : 0;
      } else if (pvNode && move.to() == prevSq && cht.probe(pos, move) > 4000)
        extensions = 1;
    }

    // Update depth;
    newDepth += extensions;

    ss->currentMove = move;
    ss->ch = &ct.table[ss->inCheck][isCapture][pos.movedPiece(move)][move.to()];

    U64 nodeCount = rootNode ? U64(nodes) : 0;
    // Make the move
    pos.makeMove(move, st);
    // Prefetch the next entry in the TT
    TTable::prefetch(tt.firstEntry(pos.key()));

    if (depth >= 2 && moveCount > 1 && !isCapture) {
      // Late move reductions
      R = 1 + (moveCount > 6) * depth / 3;
      // Don't go below depth 1
      Depth d = std::max(1, std::min(newDepth - R, newDepth));
      // Late move reductions
      value = -search<NON_PV>(pos, ss + 1, d, -alpha - 1, -alpha, true);

      // Full depth search
      if (value > alpha)
        value = -search<NON_PV>(pos, ss + 1, newDepth, -alpha - 1, -alpha,
                                !cutNode);
    } else if (!pvNode || moveCount > 1 || isCapture)
      value =
          -search<NON_PV>(pos, ss + 1, newDepth, -alpha - 1, -alpha, !cutNode);

    // Full window full depth search (PV search)
    if (pvNode && (moveCount == 1 || value > alpha)) {
      // Set new PV
      (ss + 1)->pv = pv;
      (ss + 1)->pv[0] = Move::none();

      // Recursive search
      value = -search<PV>(pos, ss + 1, newDepth, -beta, -alpha, false);
    }
    // Unmake the move
    pos.unmakeMove(move);

    // Check for stopping conditions
    if (threads.stop.load(std::memory_order_relaxed))
      return VAL_ZERO;

    // Update best move
    if (rootNode) {

      RootMove &rm = *std::find(rootMoves.begin(), rootMoves.end(), move);

      rm.effort = nodes - nodeCount;

      // PV move or new best move
      if (moveCount == 1 || value > alpha) {
        rm.score = value;
        rm.selDepth = _selDepth;

        rm.pv.resize(1);

        for (Move *m = (ss + 1)->pv; *m != Move::none(); ++m)
          rm.pv.push_back(*m);

      } else
        // All other moves but the PV are set to the lowest value
        rm.score = -VAL_INFINITE;
    }

    if (value > bestValue) {
      bestValue = value;

      if (value > alpha) {
        bestMove = move;

        if (pvNode && !rootNode)
          updatePV(ss->pv, move, (ss + 1)->pv);

        if (value >= beta)
          break;

        alpha = value;
      }
    }

    if (move != bestMove && moveCount <= 32) {
      if (pos.isCapture(move))
        captures.pushBack(move);
      else
        quiets.pushBack(move);
    }
  }

  // Check for mate/stalemate
  if (!moveCount)
    bestValue = excludedMove  ? alpha
                : ss->inCheck ? matedIn(ss->ply)
                              : VAL_ZERO;
  else if (bestMove)
    updateAllStats(ss, pos, bestMove, prevSq, captures, quiets, depth, ss->ply);
  else if (!pos.captured() && prevSq != NO_SQ)
    updateContinuations(ss - 1, pos, (ss - 1)->currentMove,
                        -statBonus(depth + 1));

  // Write to TT, save static eval
  if (!excludedMove && !rootNode)
    ttWriter.write(hashKey, TTable::valueToTT(bestValue, ss->ply), ss->ttPV,
                   bestValue >= beta    ? FLAG_LOWER
                   : pvNode && bestMove ? FLAG_EXACT
                                        : FLAG_UPPER,
                   depth, bestMove, ss->staticEval, tt._gen);

  return bestValue;
}

template <NodeType nodeType>
Value SearchWorker::qSearch(Position &pos, SearchStack *ss, Value alpha,
                            Value beta) {

  // Update node count
  constexpr bool pvNode = nodeType == PV;

  // State variables
  Move pv[MAX_PLY + 1];
  BoardState st;

  Key hashKey;
  Move move, bestMove, excludedMove;
  Value bestValue, value, futilityBase;
  int moveCount;
  bool pvHit, givesCheck;

  // Initialise node
  if (pvNode) {
    (ss + 1)->pv = pv;
    ss->pv[0] = Move::none();
  }

  // Initialise node
  Colour us = pos.sideToMove();
  ss->moveCount = moveCount = 0;
  ss->inCheck = pos.isInCheck();
  bestMove = Move::none();

  // Update node count
  nodes.fetch_add(1, std::memory_order_relaxed);

  // Update seldepth to sent to GUI
  if (pvNode)
    _selDepth = std::max(_selDepth, ss->ply);

  // Check for draws and max ply
  if (pos.isDraw(ss->ply) || ss->ply >= MAX_PLY)
    return (ss->ply >= MAX_PLY && !ss->inCheck) ? evaluate(pos)
                                                : valueDraw(nodes);

  // Transposition Table Lookup
  hashKey = pos.key();
  auto [ttHit, ttData, ttWriter] = tt.probe(hashKey);
  // Processing TT data
  ss->ttHit = ttHit;
  ttData.move = ttHit ? ttData.move : Move::none();
  ttData.value =
      ttHit ? TTable::valueFromTT(ttData.value, ss->ply, pos.fiftyMove())
            : VAL_NONE;
  pvHit = ttHit && ttData.isPV;

  // At non pv nodes, we check for early cutoffs
  if (!pvNode && ttData.depth >= DEPTH_QS && ttData.value != VAL_NONE &&
      (ttData.flag & (ttData.value >= beta ? FLAG_LOWER : FLAG_UPPER)))
    return ttData.value;

  if (ss->ttHit) {
    ss->staticEval = bestValue =
        ttData.eval != VAL_NONE ? ttData.eval : evaluate(pos);
  } else {
    // In case of null move search, use previous static eval with opposite
    // sign, as the side to move has changed and the static evaluation could
    // change drastically
    ss->staticEval = bestValue = (ss - 1)->currentMove != Move::null()
                                     ? evaluate(pos)
                                     : -(ss - 1)->staticEval;
  }

  if (bestValue >= beta) {
    if (!ss->ttHit)
      ttWriter.write(hashKey, TTable::valueToTT(bestValue, ss->ply), false,
                     FLAG_LOWER, DEPTH_UNSEARCHED, Move::none(), bestValue,
                     tt._gen);
    return bestValue;
  }

  alpha = std::max(alpha, bestValue);

  futilityBase = ss->staticEval + 300;

  Square prevSq = (ss - 1)->currentMove ? (ss - 1)->currentMove.to() : NO_SQ;

  MovePicker mp(pos, ttData.move, DEPTH_QS, ss->ply, ht, kt, cht, nullptr);

  while ((move = mp.next()) != Move::none()) {

    ss->moveCount = moveCount++;

    givesCheck = pos.givesCheck(move);

    if (bestValue > -VAL_MATE_BOUND && pos.nonPawnMaterial()) {
      // Futility pruning and moveCount pruning
      if (!givesCheck && move.to() != prevSq &&
          futilityBase > -VAL_MATE_BOUND) {
        if (moveCount > 2)
          continue;

        Value futilityValue =
            futilityBase + PieceValue[pos.capturedPiece(move)];

        // If static eval + value of the piece is much lower than alpha, then
        // the move is not beneficial
        if (futilityValue <= alpha) {
          bestValue = std::max(bestValue, futilityValue);
          continue;
        }

        // If static exchange evaluation is low enough we can prune the move.
        // (Not worth searching)
        if (pos.SEE(move, alpha - futilityBase)) {
          bestValue = std::max(bestValue, futilityBase);
          continue;
        }
      }

      // Not worth searching moves with bad enough SEE values
      if (!pos.SEE(move, -100))
        continue;
    }

    ss->currentMove = move;
    ss->ch = &ct.table[ss->inCheck][true][pos.movedPiece(move)][move.to()];

    // Make the move
    pos.makeMove(move, st);
    // Prefetch the next entry in the TT
    TTable::prefetch(tt.firstEntry(pos.key()));
    // Recursive quiescence search
    value = -qSearch<nodeType>(pos, ss + 1, -beta, -alpha);
    // Unmake the move
    pos.unmakeMove(move);

    if (value > bestValue) {
      bestValue = value;

      if (value > alpha) {
        bestMove = move;

        if (pvNode)
          updatePV(ss->pv, move, (ss + 1)->pv);

        if (value >= beta)
          break;

        alpha = value;
      }
    }
  }

  ttWriter.write(hashKey, TTable::valueToTT(bestValue, ss->ply), pvHit,
                 bestValue >= beta ? FLAG_LOWER : FLAG_UPPER, DEPTH_QS,
                 bestMove, ss->staticEval, tt._gen);

  return bestValue;
}

void SearchWorker::updateContinuations(SearchStack *ss, const Position &pos,
                                       Move move, Value bonus) {
  for (int i : {1, 2, 3, 4}) {
    if (ss->inCheck && i > 2) // Only update the first 2 entries if we are in
                              // check, as the continuation might be broken
      break;

    if ((ss - i)->currentMove)
      // Update continuation histories for the last 4 plies
      (ss - i)->ch->probe(pos, move) << bonus;
  }
}

void SearchWorker::updateAllStats(SearchStack *ss, const Position &pos,
                                  Move bestMove, Square prevSq,
                                  MoveArray &captures, MoveArray &quiets,
                                  Depth depth, int ply) {

  int bonus = statBonus(depth);

  if (!pos.isCapture(bestMove)) {
    // Update history table (Best move should receive a bonus)
    ht.update(pos, bestMove, bonus);
    // Update continuations (Best move should all receive a bonus)
    updateContinuations(ss, pos, bestMove, bonus);
    // Update killer table
    kt.update(ply, bestMove);

    for (Move m : quiets) {
      // Update history table (Non best moves should all receive a penalty)
      ht.update(pos, m, -bonus);
      // Update continuations (Non best moves should all receive a
      // penalty)
      updateContinuations(ss, pos, m, -bonus);
    }

  } else
    // Update history table (Best move should receive a bonus)
    cht.update(pos, bestMove, bonus);

  if (prevSq != NO_SQ && ((ss - 1)->moveCount == 1 + (ss - 1)->ttHit) &&
      !pos.captured())
    // Update history table (Non best moves should all receive a penalty)
    updateContinuations(ss - 1, pos, bestMove, -bonus);

  for (Move m : captures)
    // Update history table (Captures should receive a bonus)
    cht.update(pos, m, -bonus);
}

} // namespace Maestro
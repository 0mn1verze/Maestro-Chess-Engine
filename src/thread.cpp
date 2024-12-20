#include <algorithm>
#include <functional>
#include <iostream>

#include "move.hpp"
#include "movegen.hpp"
#include "search.hpp"
#include "thread.hpp"
#include "uci.hpp"

namespace Maestro {

/******************************************\
|==========================================|
|            Thread Definitions            |
|==========================================|
\******************************************/

Thread::Thread(SearchState &sharedState, size_t idx)
    : idx(idx), thread(&Thread::idleLoop, this) {
  waitForThread();

  startJob([this, &sharedState, idx] {
    worker = std::make_unique<SearchWorker>(sharedState, idx);
  });

  waitForThread();
}

Thread::~Thread() {
  exit = true;
  startSearch();
  thread.join();
}

void Thread::startSearch() {
  startJob([this] { worker->startSearch(); });
}

void Thread::clearWorker() {
  startJob([this] { worker->clear(); });
}

// Launch a function in the thread
void Thread::startJob(std::function<void()> f) {
  {
    std::unique_lock<std::mutex> lock(
        mutex); // Lock mutex to gain control to the thread
    cv.wait(lock, [&] {
      return !searching;
    }); // Wait until the thread is done searching
    jobFunc = std::move(f);
    searching = true; // Set searching flag to true
  }
  cv.notify_one(); // Notify the thread to start searching
}

void Thread::waitForThread() {
  std::unique_lock<std::mutex> lock(
      mutex); // Lock mutex to gain control to the thread
  cv.wait(lock, [&] {
    return !searching;
  }); // Wait until the thread is done searching
}

void Thread::idleLoop() {
  while (true) {
    std::unique_lock<std::mutex> lock(
        mutex);        // Lock mutex to gain control to the thread
    searching = false; // Set searching flag to false
    cv.notify_one();   // Wake up anyone waiting for search to finish
    cv.wait(lock, [&] {
      return searching;
    }); // Wait until the thread starts searching

    if (exit) // If exit flag is set, break the loop
      break;

    std::function<void()> job = std::move(jobFunc);
    jobFunc = nullptr;

    lock.unlock(); // Unlock mutex

    if (job)
      job(); // Run the job
  }
}

/******************************************\
|==========================================|
|          Thread Pool Definitions         |
|==========================================|
\******************************************/

ThreadPool::~ThreadPool() {
  if (threads.size()) {
    main()->waitForThread();
    this->clear();
  }
}

void ThreadPool::set(size_t n, SearchState &sharedState) {
  if (threads.size() > 0) {  // Destroy existing threads
    main()->waitForThread(); // Wait for main thread to finish
    clear();                 // Clear threads
    threads.clear();         // Clear thread vector
  }

  if (n > 0) {
    threads.reserve(n); // Reserve space for n threads

    while (threads.size() < n)
      threads.emplace_back(
          std::make_unique<Thread>(sharedState,
                                   threads.size())); // Create worker threads
  }

  clear();

  main()->waitForThread(); // Wait for main thread to finish
  waitForThreads();        // Wait for all threads to finish
}

void ThreadPool::clear() {

  if (threads.size() == 0)
    return; // If no threads, return

  for (auto &&th : threads)
    th->clearWorker(); // Clear thread

  for (auto &&th : threads)
    th->waitForThread(); // Wait for thread to finish

  main()->worker->bestPreviousAvgScore = VAL_INFINITE;
  main()->worker->bestPreviousScore = VAL_INFINITE;
  main()->worker->tm.clear(); // Clear time manager
}

Thread *ThreadPool::getBestThread() {
  Thread *best = main();
  Value minScore = VAL_ZERO;

  for (auto &&th : threads) {
    const int bestDepth = best->worker->completedDepth();
    const int depth = th->worker->completedDepth();

    const int bestScore = best->worker->rootMoves[0].score;
    const int score = th->worker->rootMoves[0].score;

    const auto bestThreadPV = best->worker->rootMoves[0].pv;
    const auto threadPV = th->worker->rootMoves[0].pv;

    // If the depth is equal to the best depth and the score is greater, or this
    // thread found a mate that is better than the best thread, set the best
    // thread to the current thread
    if ((depth == bestDepth && score > bestScore) ||
        (score > VAL_MATE_BOUND && score > bestScore))
      best = th.get();

    // If the depth is greater than the best depth and the best score isn't
    // mate, set the best thread to the current thread
    if (depth > bestDepth && (score > bestScore || bestScore < VAL_MATE_BOUND))
      best = th.get();
  }

  return best;
}

struct Limits;

void ThreadPool::startThinking(Position &pos, StateListPtr &s, Limits limits) {

  main()->waitForThread(); // Wait for main thread to finish

  RootMoves rootMoves;
  const auto legalMoves = MoveList<ALL>(pos);

  for (const Move &m : legalMoves)
    rootMoves.emplace_back(m);

  if (s.get())
    states = std::move(s);

  for (auto &&th : threads) {
    th->startJob([&] {
      th->worker->limits = limits;
      th->worker->nodes = 0;
      th->worker->rootDepth = 0;
      th->worker->rootMoves = rootMoves;
      th->worker->rootPos.set(pos.fen(), th->worker->rootState);
      th->worker->rootState = states->back();
    });
  }

  main()->waitForThread(); // Wait for main thread to finish
  waitForThreads();        // Wait for all threads to finish

  main()->startSearch(); // Start main thread
}

void ThreadPool::startJob(size_t threadId, std::function<void()> f) {
  threads.at(threadId)->startJob(std::move(f));
}

void ThreadPool::waitForThread(size_t threadId) {
  threads.at(threadId)->waitForThread();
}

void ThreadPool::startSearch() {
  for (auto &&th : threads)
    if (th.get() != main())
      th->startSearch();
}
void ThreadPool::waitForThreads() {
  for (auto &&th : threads)
    if (th.get() != main())
      th->waitForThread();
}

// Return nodes searched
U64 ThreadPool::nodesSearched() const {
  return accumulate(&SearchWorker::nodes);
}

} // namespace Maestro
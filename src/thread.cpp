#include <algorithm>
#include <functional>

#include "move.hpp"
#include "search.hpp"
#include "thread.hpp"

namespace Maestro {

/******************************************\
|==========================================|
|            Thread Definitions            |
|==========================================|
\******************************************/

Thread::Thread(SearchState &sharedState, size_t idx)
    : idx(idx), thread(&Thread::idleLoop, this) {
  waitForThread();

  startCustomJob([this, &sharedState, idx] {
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
  startCustomJob([this] { worker->startSearch(); });
}

void Thread::clearWorker() {
  startCustomJob([this] { worker->clear(); });
}

// Launch a function in the thread
void Thread::startCustomJob(std::function<void()> f) {
  std::unique_lock<std::mutex> lock(
      mutex); // Lock mutex to gain control to the thread
  cv.wait(lock, [&] {
    return !searching;
  }); // Wait until the thread is done searching
  jobFunc = std::move(f);
  searching = true; // Set searching flag to true
  cv.notify_one();  // Notify the thread to start searching
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
  if (size()) {
    main()->waitForThread();
    this->clear();
  }
}

void ThreadPool::set(size_t n, SearchState sharedState) {
  if (size() > 0) {          // Destroy existing threads
    main()->waitForThread(); // Wait for main thread to finish
    clear();

    for (auto &&th : *this)
      delete th.get(); // Delete thread
  }

  if (n > 0) {
    reserve(n); // Reserve space for n threads

    while (size() < n)
      emplace_back(std::make_unique<Thread>(sharedState,
                                            n)); // Create worker threads
  }

  main()->waitForThread(); // Wait for main thread to finish
}

void ThreadPool::clear() {

  if (size() == 0)
    return; // If no threads, return

  for (auto &&th : *this)
    th->clearWorker(); // Clear thread

  for (auto &&th : *this)
    th->waitForThread(); // Wait for thread to finish
}

struct Limits;

void ThreadPool::startThinking(Position &pos, StateListPtr &s, Limits limits) {
  main()->waitForThread(); // Wait for main thread to finish

  RootMoves rootMoves;
  const auto legalMoves = MoveList<ALL>(pos);

  if (rootMoves.empty())
    for (const Move &m : legalMoves)
      rootMoves.emplace_back(m);

  if (states.get())
    states = std::move(s);

  BoardState tmp = states->back();

  for (auto &&th : *this) {
    th->startCustomJob([&] {
      th->worker->limits = limits;
      th->worker->nodes = th->worker->tbHits = th->worker->bestMoveChanges = 0;
      th->worker->rootDepth = 0;
      th->worker->rootMoves = rootMoves;
      th->worker->rootPos.set(pos.fen(), th->worker->rootState, th.get());
      th->worker->rootState = states->back();
    });
  }

  states->back() = tmp;

  main()->startSearch(); // Start main thread
}

void ThreadPool::startCustomJob(size_t threadId, std::function<void()> f) {
  at(threadId)->startCustomJob(std::move(f));
}

void ThreadPool::waitForThread(size_t threadId) {
  at(threadId)->waitForThread();
}

void ThreadPool::startSearch() {
  for (auto &&th : *this)
    if (th.get() != main())
      th->startSearch();
}
void ThreadPool::waitForThreads() {
  for (auto &&th : *this)
    if (th.get() != main())
      th->waitForThread();
}

// Return nodes searched
U64 ThreadPool::nodesSearched() const {
  return accumulate(&SearchWorker::nodes);
}

// Return table hits
U64 ThreadPool::tbHits() const { return accumulate(&SearchWorker::tbHits); }

} // namespace Maestro
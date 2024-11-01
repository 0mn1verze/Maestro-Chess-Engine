#include <algorithm>
#include <functional>

#include "move.hpp"
#include "thread.hpp"

namespace Maestro {

Thread::Thread(size_t idx) : idx(idx), thread(&Thread::idleLoop, this) {
  waitForThread();
}

Thread::~Thread() {
  exit = true;
  startSearch();
  thread.join();
}

void Thread::clear() {
  killerTable.fill(GenMove::none());
  counterMoveTable.fill(GenMove::none());
  historyTable.fill(0);
  captureHistoryTable.fill(0);
}

void Thread::startSearch() {
  startCustomJob([this] { search(); });
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

void ThreadPool::set(size_t n) {
  if (size() > 0) {          // Destroy existing threads
    main()->waitForThread(); // Wait for main thread to finish
    while (size() > 0)
      delete back(), pop_back(); // Delete threads
  }

  if (n > 0) {
    reserve(n); // Reserve space for n threads

    push_back(new MainThread(0)); // Create main thread

    while (size() < n)
      push_back(new Thread(size())); // Create worker threads
  }
}

void ThreadPool::clear() {
  for (Thread *th : *this)
    th->clear(); // Clear thread
}

void ThreadPool::init(Position &pos, StateListPtr &s) {
  main()->waitForThread(); // Wait for main thread to finish

  if (states.get())
    states = std::move(s);

  BoardState tmp = states->back();

  for (Thread *th : *this) {
    th->nodes = th->tbHits = 0;                     // Reset nodes and tbHits
    th->rootDepth = 0;                              // Reset root depth
    th->rootPos.set(pos.fen(), states->back(), th); // Set root position
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
  for (Thread *th : *this)
    if (th != main())
      th->startSearch();
}
void ThreadPool::waitForThreads() {
  for (Thread *th : *this)
    if (th != main())
      th->waitForThread();
}

void Thread::search() {}

void MainThread::search() {}

} // namespace Maestro
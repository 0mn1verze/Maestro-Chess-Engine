#ifndef THREAD_HPP
#pragma once
#define THREAD_HPP

#include <atomic>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <thread>

#include "defs.hpp"
#include "move.hpp"

#include "position.hpp"
#include "search.hpp"
#include "utils.hpp"

namespace Maestro {

/******************************************\
|==========================================|
|            Thread Definitions            |
|==========================================|
\******************************************/

class Thread {

public:
  Thread(SearchState &, size_t);
  virtual ~Thread();

  void clearWorker();
  void idleLoop();
  void startSearch();
  void waitForThread();
  void startJob(std::function<void()> f);

  std::unique_ptr<SearchWorker> worker;
  std::function<void()> jobFunc;

private:
  std::mutex mutex;
  std::condition_variable cv;
  size_t idx;
  bool exit = false, searching = true;
  std::thread thread;
};

/******************************************\
|==========================================|
|          Thread Pool Definitions         |
|==========================================|
\******************************************/

class ThreadPool {
public:
  ThreadPool(){};
  ~ThreadPool();

  ThreadPool(const ThreadPool &) = delete;
  ThreadPool(ThreadPool &&) = delete;

  ThreadPool &operator=(const ThreadPool &) = delete;
  ThreadPool &operator=(ThreadPool &&) = delete;

  void startThinking(Position &pos, StateListPtr &states, Limits limits);
  void clear();
  size_t size() const { return threads.size(); }
  void set(size_t n, SearchState &sharedState);

  void startJob(size_t threadId, std::function<void()> f);
  void waitForThread(size_t threadId);
  void startSearch();
  void waitForThreads();
  Thread *getBestThread();

  Thread *main() const { return threads.front().get(); }
  U64 nodesSearched() const;

  std::atomic_bool stop, abortedSearch;

private:
  StateListPtr states;
  std::vector<std::unique_ptr<Thread>> threads;

  U64 accumulate(std::atomic<U64> SearchWorker::*member) const {
    U64 sum = 0;
    for (auto &&t : threads)
      sum += (t->worker.get()->*member).load(std::memory_order_relaxed);
    return sum;
  }
};

} // namespace Maestro

#endif // THREAD_HPP
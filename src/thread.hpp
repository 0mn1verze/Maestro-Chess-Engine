#ifndef THREAD_HPP
#define THREAD_HPP

#include <atomic>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <thread>

#include "defs.hpp"
#include "move.hpp"
#include "movepicker.hpp"
#include "position.hpp"
#include "utils.hpp"

/******************************************\
|==========================================|
|            Thread Definitions            |
|==========================================|
\******************************************/

class Thread {
  std::mutex mutex;
  std::condition_variable cv;
  size_t idx;
  bool exit = false, searching = true;
  std::thread thread;

public:
  explicit Thread(size_t);
  virtual ~Thread();
  virtual void search();
  void clear();
  void idleLoop();
  void startSearch();
  void waitForThread();
  void startCustomJob(std::function<void()> f);

  size_t pvIdx, pvLast;
  int selfDepth;
  std::atomic<U64> nodes, tbHits, bestMoveChanges;

  Position rootPos;
  U16 rootDepth;

  KillerTable killerTable;
  CounterMoveTable counterMoveTable;
  HistoryTable historyTable;
  CaptureHistoryTable captureHistoryTable;
  ContinuationTable continuationTable;

  std::function<void()> jobFunc;
};

class MainThread : public Thread {

  using Thread::Thread; // Inherit constructor and keeping explicit constructor

  void search() override;
  void check_time();
};

class ThreadPool : public std::vector<Thread *> {
public:
  void init(Position &pos, StateListPtr &states);
  void clear();
  void set(size_t n);

  void startCustomJob(size_t threadId, std::function<void()> f);
  void waitForThread(size_t threadId);
  void startSearch();
  void waitForThreads();

  MainThread *main() const { return static_cast<MainThread *>(front()); }
  U64 nodes_searched() const { return accumulate(&Thread::nodes); }
  U64 tb_hits() const { return accumulate(&Thread::tbHits); }

  std::atomic_bool stop;

private:
  StateListPtr states;

  U64 accumulate(std::atomic<U64> Thread::*member) const {
    U64 sum = 0;
    for (auto &&t : *this)
      sum += (t->*member).load(std::memory_order_relaxed);
    return sum;
  }
};

#endif // THREAD_HPP
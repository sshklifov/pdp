#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "system/thread.h"

using namespace pdp;

struct NoOpTask {
  std::atomic<bool> *ran;

  explicit NoOpTask(std::atomic<bool> *r) : ran(r) {}
  void operator()() { ran->store(true, std::memory_order_relaxed); }
};

TEST_CASE("Thread: no-op thread runs and joins") {
  Thread t;

  std::atomic<bool> ran = false;

  t.Start(NoOpTask(&ran));
  t.Wait();

  CHECK(ran.load(std::memory_order_relaxed));
}

TEST_CASE("Thread: real workload increments atomic counter") {
  struct IncrementTask {
    std::atomic<int> *counter;

    void operator()() {
      for (int i = 0; i < 100'000; ++i) {
        counter->fetch_add(1, std::memory_order_relaxed);
      }
    }
  };

  Thread t;

  std::atomic<int> counter = 0;
  IncrementTask task{&counter};

  t.Start(task);
  t.Wait();

  CHECK(counter.load(std::memory_order_relaxed) == 100'000);
}

TEST_CASE("StoppableThread: stop interrupts running thread") {
  struct StoppableTask {
    void operator()(std::atomic_bool *running, std::atomic<int> *iterations) {
      while (running->load(std::memory_order_relaxed)) {
        iterations->fetch_add(1, std::memory_order_relaxed);
      }
    }
  };

  StoppableThread t;

  std::atomic<int> iterations{0};
  StoppableTask task;

  t.Start(task, &iterations);

  while (iterations.load(std::memory_order_relaxed) == 0) {
  }

  t.Stop();

  CHECK(iterations.load(std::memory_order_relaxed) > 0);
}

// TODO something with more exotic arguments / references etc.

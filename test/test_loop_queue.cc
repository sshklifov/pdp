#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "data/loop_queue.h"

using pdp::LoopQueue;

[[maybe_unused]]
void PrintLoopQueue(LoopQueue<int, pdp::DefaultAllocator> &q) {
  printf("[debug] queue: ");
  for (size_t i = 0; i < q.Size(); ++i) {
    printf("%d, ", q.At(i));
  }
  printf("\n");
}

TEST_CASE("LoopQueue basic push and access") {
  LoopQueue<int> q(4);

  CHECK(q.Empty());
  CHECK(q.Size() == 0);

  q.EmplaceBack(1);
  q.EmplaceBack(2);
  q.EmplaceBack(3);

  CHECK(!q.Empty());
  CHECK(q.Size() == 3);
  CHECK(q.Front() == 1);
  CHECK(q.At(0) == 1);
  CHECK(q.At(1) == 2);
  CHECK(q.At(2) == 3);
}

TEST_CASE("LoopQueue wraparound correctness") {
  LoopQueue<int> q(4);

  q.EmplaceBack(1);
  q.EmplaceBack(2);
  q.EmplaceBack(3);
  q.EmplaceBack(4);

  // force wrap by pushing front
  q.EmplaceFront(0);

  CHECK(q.Size() == 5);
  CHECK(q.Front() == 0);
  CHECK(q.At(0) == 0);
  CHECK(q.At(1) == 1);
  CHECK(q.At(2) == 2);
  CHECK(q.At(3) == 3);
  CHECK(q.At(4) == 4);
}

TEST_CASE("LoopQueue grow preserves order") {
  LoopQueue<int> q(2);

  for (int i = 0; i < 10; ++i) {
    q.EmplaceBack(i);
  }

  CHECK(q.Size() == 10);
  for (int i = 0; i < 10; ++i) {
    CHECK(q.At(i) == i);
  }

  CHECK(q.Front() == 0);
}

struct Tracked {
  int value;
  bool moved;
  static inline int alive = 0;

  explicit Tracked(int v) : value(v), moved(false) { ++alive; }
  Tracked(Tracked &&o) noexcept : value(o.value), moved(false) { o.moved = true; }
  Tracked(const Tracked &o) : value(o.value) { ++alive; }

  ~Tracked() {
    if (!moved) {
      --alive;
    }
  }
};

TEST_CASE("LoopQueue element lifetime correctness") {
  {
    LoopQueue<Tracked> q(4);
    for (int i = 0; i < 8; ++i) {
      q.EmplaceBack(Tracked{i});
    }
    CHECK(Tracked::alive == 8);
  }

  CHECK(Tracked::alive == 0);
}

TEST_CASE("LoopQueue alternating front/back preserves order") {
  LoopQueue<int> q(4);

  std::vector<int> expected;

  for (int i = 0; i < 20; ++i) {
    if (i % 2 == 0) {
      q.EmplaceFront(i);
      expected.insert(expected.begin(), i);
    } else {
      q.EmplaceBack(i);
      expected.push_back(i);
    }
  }

  REQUIRE(q.Size() == expected.size());

  // Check full linear order
  for (size_t i = 0; i < expected.size(); ++i) {
    CHECK(q.At(i) == expected[i]);
  }

  // Sanity-check ends
  CHECK(q.Front() == expected.front());
  CHECK(q.Back() == expected.back());
}

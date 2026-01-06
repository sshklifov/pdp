#include <random>
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "data/callback_table.h"  // whatever file name this garbage lives in (WOW CHATGPT!)

using namespace pdp;

// ------------------------------------------------------------
// Helpers
// ------------------------------------------------------------

struct CountCallback {
  int *ptr;
  explicit CountCallback(int *p) : ptr(p) {}
  void operator()(int v) { *ptr += v; }
};

// ------------------------------------------------------------
// Tests
// ------------------------------------------------------------

TEST_CASE("basic bind + invoke (integral context)") {
  CallbackTable<int> table;

  int result = 0;
  table.Bind<CountCallback>(42, &result);

  CHECK(table.Invoke(42, 5));
  CHECK(result == 5);
}

TEST_CASE("destructor is called exactly once") {
  CallbackTable<int> table;

  struct DestructorSpy {
    bool *destroyed;

    DestructorSpy(bool *d) : destroyed(d) {}

    void operator()(int) {}
    ~DestructorSpy() { *destroyed = 1; }
  };

  bool destroyed = 0;

  table.Bind<DestructorSpy>(7, &destroyed);

  CHECK(table.Invoke(7, 0));
  CHECK(destroyed == 1);
}

TEST_CASE("multiple callbacks with different ids") {
  CallbackTable<int> table;

  int a = 0;
  int b = 0;

  table.Bind<CountCallback>(10, &a);
  table.Bind<CountCallback>(20, &b);

  CHECK(table.Invoke(20, 7));
  CHECK(b == 7);
  CHECK(a == 0);

  CHECK(table.Invoke(10, 4));
  CHECK(a == 4);
}

TEST_CASE("context as pointer") {
  CallbackTable<uint64_t *> table;

  uint64_t value = 123;
  uint64_t out = 0;

  struct PtrCallback {
    uint64_t *out;
    explicit PtrCallback(uint64_t *o) : out(o) {}
    void operator()(uint64_t *p) { *out = *p; }
  };

  table.Bind<PtrCallback>(99, &out);
  CHECK(table.Invoke(99, &value));
  CHECK(out == 123);
}

TEST_CASE("table grows correctly") {
  CallbackTable<int> table;

  static constexpr int N = 64;
  int sum = 0;

  for (int i = 0; i < N; ++i) {
    table.Bind<CountCallback>(i, &sum);
  }

  for (int i = 0; i < N; ++i) {
    CHECK(table.Invoke(i, 1));
  }

  CHECK(sum == N);
}

TEST_CASE("Callback capture reference") {
  CallbackTable<> table;

  int value = 0;
  struct IncrementCallback {
    IncrementCallback(int &v) : v(v) {}

    void operator()() { v += 1; }

   private:
    int &v;
  };

  table.Bind<IncrementCallback>(4, value);

  struct IncrementTwiceCallback {
    IncrementTwiceCallback(int &v) : v(v) {}

    void operator()() { v += 2; }

   private:
    int &v;
  };

  table.Bind<IncrementTwiceCallback>(5, value);

  table.Invoke(4);
  table.Invoke(5);
  CHECK(value == 3);
}

TEST_CASE("Flow-style stress test with holes, reuse, and random invokes") {
  CallbackTable<int> table;

  static constexpr uint32_t kTotalIds = 100'000;
  static constexpr uint32_t kMaxActive = 100;

  uint64_t expected_hash = 0x9E3779B97F4A7C15;
  uint64_t actual_hash = 0x9E3779B97F4A7C15;

  // Track active IDs
  std::vector<uint32_t> active;
  active.reserve(kMaxActive);

  // Deterministic RNG (important for reproducibility)
  std::mt19937 rng(0xdeadbeef);

  struct AddId {
    uint64_t *out;
    uint32_t id;

    AddId(uint64_t *out, uint32_t id) : out(out), id(id) {}
    void operator()(int) { *out += id; }
  };

  uint32_t next_id = 1;

  auto bind_one = [&]() {
    REQUIRE(active.size() < kMaxActive);
    REQUIRE(next_id <= kTotalIds);

    uint32_t id = next_id++;
    expected_hash += id;

    table.Bind<AddId>(id, &actual_hash, id);
    active.push_back(id);
  };

  auto invoke_random = [&]() {
    REQUIRE(!active.empty());

    std::uniform_int_distribution<size_t> pick(0, active.size() - 1);
    size_t idx = pick(rng);

    uint32_t id = active[idx];
    CHECK(table.Invoke(id, 0));

    // remove by swap+pop (creates holes & randomness)
    active[idx] = active.back();
    active.pop_back();
  };

  // Main flow
  while (next_id <= kTotalIds || !active.empty()) {
    bool can_bind = next_id <= kTotalIds && active.size() < kMaxActive;
    bool can_invoke = !active.empty();

    // Decide action
    if (can_bind && can_invoke) {
      // Bias slightly toward bind to keep pressure on holes
      if ((rng() & 1) == 0) {
        bind_one();
      } else {
        invoke_random();
      }
    } else if (can_bind) {
      bind_one();
    } else {
      invoke_random();
    }
  }

  // Final validation
  CHECK(active.empty());
  CHECK(next_id == kTotalIds + 1);
  CHECK(actual_hash == expected_hash);
}

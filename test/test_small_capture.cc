#include "data/scoped_ptr.h"
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "data/small_capture.h"

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

TEST_CASE("SmallCapture: basic bind + invoke") {
  SmallCapture<int> cap;

  int result = 0;
  cap.Bind<CountCallback>(&result);

  cap(5);
  CHECK(result == 5);
}

TEST_CASE("SmallCapture: pointer argument") {
  SmallCapture<uint64_t *> cap;

  uint64_t value = 123;
  uint64_t out = 0;

  struct PtrCallback {
    uint64_t *out;
    explicit PtrCallback(uint64_t *o) : out(o) {}
    void operator()(uint64_t *p) { *out = *p; }
  };

  cap.Bind<PtrCallback>(&out);
  cap(&value);

  CHECK(out == 123);
}

TEST_CASE("SmallCapture: reference capture") {
  SmallCapture<> cap;

  int value = 0;

  struct Increment {
    int &v;
    explicit Increment(int &v) : v(v) {}
    void operator()() { ++v; }
  };

  cap.Bind<Increment>(value);
  cap();

  CHECK(value == 1);
}

TEST_CASE("SmallCapture: multiple arguments") {
  SmallCapture<int, int> cap;

  int result = 0;

  struct Add {
    int *out;
    explicit Add(int *o) : out(o) {}
    void operator()(int a, int b) { *out = a + b; }
  };

  cap.Bind<Add>(&result);
  cap(3, 4);

  CHECK(result == 7);
}

TEST_CASE("SmallCapture: move-only argument is forwarded") {
  SmallCapture<ScopedPtr<int>> cap;

  int result = 0;

  struct Consume {
    int *out;
    explicit Consume(int *o) : out(o) {}
    void operator()(ScopedPtr<int> &&p) { *out = *p; }
  };

  cap.Bind<Consume>(&result);

  ScopedPtr<int> ptr((int *)malloc(sizeof(int)));
  *ptr = 42;
  cap(std::move(ptr));

  CHECK(result == 42);
  CHECK(!ptr);
}

TEST_CASE("SmallCapture: reuse after invoke") {
  SmallCapture<int> cap;

  int a = 0;
  cap.Bind<CountCallback>(&a);
  cap(1);
  CHECK(a == 1);

  int b = 0;
  cap.Bind<CountCallback>(&b);
  cap(2);
  CHECK(b == 2);
}

TEST_CASE("SmallCapture: stress bind/invoke cycles") {
  SmallCapture<int> cap;

  static constexpr int N = 10000;
  int sum = 0;

  struct Add {
    int *out;
    explicit Add(int *o) : out(o) {}
    void operator()(int v) { *out += v; }
  };

  for (int i = 0; i < N; ++i) {
    cap.Bind<Add>(&sum);
    cap(1);
  }

  CHECK(sum == N);
}

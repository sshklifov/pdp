#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "system/time_units.h"

using namespace pdp;

TEST_CASE("Milliseconds: comparisons") {
  Milliseconds a(10);
  Milliseconds b(20);

  CHECK(a < b);
  CHECK(b > a);
  CHECK(a <= b);
  CHECK(b >= a);
  CHECK(a != b);
  CHECK(Milliseconds(10) == a);
}

TEST_CASE("Milliseconds: arithmetic") {
  Milliseconds a(50);
  Milliseconds b(20);

  CHECK((a - b).Get() == 30);
  CHECK((a + b).Get() == 70);

  a -= b;
  CHECK(a.Get() == 30);

  a += b;
  CHECK(a.Get() == 50);
}

TEST_CASE("Milliseconds: user-defined literal") {
  auto t = 150_ms;
  CHECK(t.Get() == 150);
}

TEST_CASE("Stopwatch: elapsed time increases") {
  Stopwatch sw;

  Milliseconds t1 = sw.Elapsed();
  usleep(10'000);
  Milliseconds t2 = sw.Elapsed();

  CHECK(t2 >= t1 + Milliseconds(10));
}

TEST_CASE("Stopwatch: reset works") {
  Stopwatch sw;

  usleep(10'000);
  sw.Reset();
  Milliseconds after = sw.Elapsed();

  // After reset, elapsed should be smaller
  CHECK(after <= Milliseconds(5));
}

TEST_CASE("Stopwatch: elapsed is non-negative") {
  Stopwatch sw;
  Milliseconds t = sw.Elapsed();

  CHECK(t.Get() >= 0);
}

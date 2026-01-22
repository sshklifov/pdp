#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "data/allocator.h"
#include "data/vector.h"
#include "strings/dynamic_string.h"

using pdp::TrackingAllocator;
using pdp::Vector;

/* ===============================
   Compile-time contract tests
   =============================== */

TEST_CASE("Vector type traits") {
  static_assert(!std::is_copy_constructible_v<Vector<int>>);
  static_assert(!std::is_copy_assignable_v<Vector<int>>);

  static_assert(std::is_move_constructible_v<Vector<int>>);

  static_assert(noexcept(Vector<int>{}));
}

/* ===============================
   Basic construction
   =============================== */

TEST_CASE("Default constructed vector") {
  Vector<int> v;

  CHECK(v.Data() == nullptr);
  CHECK(v.Size() == 0);
  CHECK(v.Capacity() == 0);
  CHECK(v.Empty());
}

TEST_CASE("Capacity constructor") {
  Vector<int> v(8);

  CHECK(v.Data() != nullptr);
  CHECK(v.Size() == 0);
  CHECK(v.Capacity() == 8);
  CHECK(v.Empty());
}

/* ===============================
   Push / append behavior
   =============================== */

TEST_CASE("Append elements with operator +=") {
  Vector<int> v(2);

  v += 1;
  v += 2;

  CHECK(v.Size() == 2);
  CHECK(v[0] == 1);
  CHECK(v[1] == 2);
}

TEST_CASE("Append triggers growth") {
  Vector<int> v(1);

  v += 10;
  auto old_capacity = v.Capacity();

  v += 20;  // must grow

  CHECK(v.Size() == 2);
  CHECK(v[0] == 10);
  CHECK(v[1] == 20);
  CHECK(v.Capacity() > old_capacity);
}

/* ===============================
   NewElement API
   =============================== */

TEST_CASE("NewElement reserves space but does not construct") {
  Vector<uint32_t> v(1);

  uint32_t *p = v.NewElement();
  REQUIRE(p != nullptr);

  *p = 42;

  CHECK(v.Size() == 1);
  CHECK(v[0] == 42);
}

/* ===============================
   Move semantics
   =============================== */

TEST_CASE("Move constructor transfers ownership") {
  TrackingAllocator::Stats stats;
  TrackingAllocator alloc(&stats);

  {
    Vector<int, TrackingAllocator> a(4, alloc);
    a += 1;
    a += 2;

    auto ptr = a.Data();

    Vector<int, TrackingAllocator> b(std::move(a));

    CHECK(b.Data() == ptr);
    CHECK(b.Size() == 2);
    CHECK(b[0] == 1);
    CHECK(b[1] == 2);
  }

  CHECK(!stats.HasLeaks());
}

TEST_CASE("Vector preserves elements across growth and updates allocator counters") {
  TrackingAllocator::Stats stats;
  TrackingAllocator alloc(&stats);

  // Start with small capacity to force growth.
  Vector<uint32_t, TrackingAllocator> v(2, alloc);

  // One allocation for initial buffer.
  CHECK(stats.GetActiveAllocations() == 1);
  CHECK(stats.GetBytesUsed() > 0);

  // Fill and remember the current buffer.
  v += 0xAABBCCDDu;
  v += 0x11223344u;

  auto bytes_before = stats.GetBytesUsed();
  auto allocs_before = stats.GetActiveAllocations();

  // This push must trigger GrowExtra -> ReallocateRaw.
  v += 0xDEADBEEFu;

  CHECK(v.Size() == 3);

  // Elements must remain correct after reallocation.
  CHECK(v[0] == 0xAABBCCDDu);
  CHECK(v[1] == 0x11223344u);
  CHECK(v[2] == 0xDEADBEEFu);

  // Counters: realloc shouldn't change allocation count for a typical realloc-based allocator.
  CHECK(stats.GetActiveAllocations() == allocs_before);
  CHECK(stats.GetActiveAllocations() == 1);

  // Bytes should generally grow (unless allocator rounds in weird ways).
  CHECK(stats.GetBytesUsed() >= bytes_before);

  // Now destroy: should release the buffer => 0 allocations, 0 bytes.
  v.Destroy();
  CHECK(!stats.HasLeaks());
}

TEST_CASE("Vector grows exponentially") {
  TrackingAllocator::Stats stats{};
  TrackingAllocator alloc(&stats);

  Vector<uint32_t, TrackingAllocator> v(1, alloc);

  constexpr size_t max_elements = 131072;

  for (size_t i = 0; i < max_elements; ++i) {
    v += i;
  }

  // Exponential growth ⇒ reallocations should be logarithmic
  CHECK(stats.GetAllocationsMade() < 30);

  // Only one live allocation at the end
  CHECK(stats.GetActiveAllocations() == 1);

  v.Destroy();
  CHECK(!stats.HasLeaks());
}

/* ===============================
   Reserve behavior
   =============================== */

TEST_CASE("ReserveFor does not change size") {
  Vector<int> v(2);
  v += 1;

  v.ReserveFor(10);

  CHECK(v.Size() == 1);
  CHECK(v.Capacity() >= 11);
  CHECK(v[0] == 1);
}

/* ===============================
   Destroy semantics
   =============================== */

TEST_CASE("Destroy resets vector") {
  Vector<int> v(3);
  v += 1;
  v += 2;

  v.Destroy();

  CHECK(v.Data() == nullptr);
  CHECK(v.Size() == 0);
  CHECK(v.Capacity() == 0);
}

TEST_CASE("Emplace constructs element and increases size") {
  Vector<int> v(2);

  v.Emplace(42);

  CHECK(v.Size() == 1);
  CHECK(v.Capacity() >= 1);
  CHECK(v[0] == 42);
}

struct TestPair {
  TestPair(pdp::DynamicString &&first, int second) : first(std::move(first)), second(second) {}

  pdp::DynamicString first;
  int second;
};

template <>
struct pdp::IsReallocatable<TestPair> : std::true_type {};

TEST_CASE("Emplace constructs simple struct and appends it") {
  Vector<TestPair> v(2);

  pdp::DynamicString s("Test string", 11);
  const void *old_ptr = s.Begin();
  v.Emplace(std::move(s), 2);

  CHECK(v.Size() == 1);
  CHECK(v[0].first == "Test string");
  CHECK(v[0].second == 2);

  CHECK(old_ptr == v[0].first.Data());
  CHECK(s.Begin() == nullptr);
}

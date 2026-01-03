#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "data/chunk_array.h"

using namespace pdp;

TEST_CASE("ChunkArray basic allocation returns aligned pointers") {
  ChunkArray<> ca;

  void *p = ca.Allocate(8);
  REQUIRE(p);

  CHECK(reinterpret_cast<uintptr_t>(p) % ChunkArray<>::alignment == 0);
}

TEST_CASE("ChunkArray allocations inside a chunk advance by exact alignment") {
  ChunkArray<> ca;
  auto a = ChunkArray<>::alignment;

  void *p1 = ca.Allocate(1);
  void *p2 = ca.Allocate(1);
  void *p3 = ca.Allocate(1);

  REQUIRE(p1);
  REQUIRE(p2);
  REQUIRE(p3);

  CHECK(reinterpret_cast<uintptr_t>(p2) - reinterpret_cast<uintptr_t>(p1) == a);

  CHECK(reinterpret_cast<uintptr_t>(p3) - reinterpret_cast<uintptr_t>(p2) == a);
}

TEST_CASE("ChunkArray rounds up misaligned sizes correctly") {
  ChunkArray<> ca;
  auto a = ChunkArray<>::alignment;

  void *p1 = ca.Allocate(3);
  void *p2 = ca.Allocate(5);
  void *p3 = ca.Allocate(7);

  REQUIRE(p1);
  REQUIRE(p2);
  REQUIRE(p3);

  CHECK(reinterpret_cast<uintptr_t>(p2) - reinterpret_cast<uintptr_t>(p1) == a);
  CHECK(reinterpret_cast<uintptr_t>(p3) - reinterpret_cast<uintptr_t>(p2) == a);
}

TEST_CASE("ChunkArray fills chunk exactly then allocates new chunk") {
  ChunkArray<> ca;

  constexpr size_t chunk = ChunkArray<>::chunk_size;
  constexpr size_t stride = 16;

  static_assert(chunk % stride == 0);

  void *first = nullptr;
  void *last = nullptr;

  size_t count = chunk / stride;

  for (size_t i = 0; i < count; ++i) {
    void *p = ca.Allocate(stride);
    REQUIRE(p);

    if (i == 0) first = p;
    last = p;
  }

  // Last allocation must still be inside same chunk
  CHECK(reinterpret_cast<uintptr_t>(last) - reinterpret_cast<uintptr_t>(first) ==
        (count - 1) * stride);
  CHECK(ca.NumChunks() == 1);

  // Next allocation MUST be from a new chunk
  void *spill = ca.Allocate(stride);
  REQUIRE(spill);
  CHECK(ca.NumChunks() == 2);
}

TEST_CASE("ChunkArray allocates large blocks directly") {
  ChunkArray<> ca;
  auto a = ChunkArray<>::alignment;

  constexpr size_t big = 128_KB;

  CHECK(ca.NumChunks() == 1);
  void *p1 = ca.Allocate(big);
  CHECK(ca.NumChunks() == 2);
  void *p2 = ca.Allocate(big);
  CHECK(ca.NumChunks() == 3);

  REQUIRE(p1);
  REQUIRE(p2);

  CHECK(reinterpret_cast<uintptr_t>(p1) % a == 0);
  CHECK(reinterpret_cast<uintptr_t>(p2) % a == 0);
}

TEST_CASE("ChunkArray survives small-large-small allocation sequence") {
  ChunkArray<> ca;
  auto a = ChunkArray<>::alignment;

  CHECK(ca.NumChunks() == 1);
  void *s1 = ca.Allocate(8);
  CHECK(ca.NumChunks() == 1);
  void *big = ca.Allocate(128_KB);
  CHECK(ca.NumChunks() == 2);
  void *s2 = ca.Allocate(8);
  CHECK(ca.NumChunks() == 2);

  REQUIRE(s1);
  REQUIRE(big);
  REQUIRE(s2);

  CHECK(reinterpret_cast<uintptr_t>(s1) % a == 0);
  CHECK(reinterpret_cast<uintptr_t>(big) % a == 0);
  CHECK(reinterpret_cast<uintptr_t>(s2) % a == 0);

  CHECK(s1 != s2);
  CHECK(big != s2);
  CHECK(reinterpret_cast<uintptr_t>(s2) - reinterpret_cast<uintptr_t>(s1) == 8);
}

TEST_CASE("ChunkArray AllocateOrNull returns nullptr for zero") {
  ChunkArray<> ca;

  void *p = ca.AllocateOrNull(0);
  CHECK(p == nullptr);
}

TEST_CASE("ChunkArray always returns aligned pointers even for garbage sizes") {
  ChunkArray<> arena;

  uint32_t sizes[] = {1, 2, 3, 5, 7, 9, 15, 31};

  for (uint32_t sz : sizes) {
    void *p = arena.Allocate(sz);
    REQUIRE(p);
    CHECK(reinterpret_cast<uintptr_t>(p) % ChunkArray<>::alignment == 0);
  }
}

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "data/arena.h"

using namespace pdp;

TEST_CASE("Arena basic allocation and monotonic growth") {
  constexpr size_t cap = 1024;
  Arena<> arena(cap);

  void *p1 = arena.Allocate(16);
  void *p2 = arena.Allocate(16);
  void *p3 = arena.Allocate(32);

  REQUIRE(p1 != nullptr);
  REQUIRE(p2 != nullptr);
  REQUIRE(p3 != nullptr);

  CHECK(reinterpret_cast<uintptr_t>(p2) > reinterpret_cast<uintptr_t>(p1));
  CHECK(reinterpret_cast<uintptr_t>(p3) > reinterpret_cast<uintptr_t>(p2));
}

TEST_CASE("Arena enforces alignment") {
  constexpr size_t cap = 1024;
  Arena<> arena(cap);

  void *p = arena.Allocate(8);
  REQUIRE(p != nullptr);

  CHECK(reinterpret_cast<uintptr_t>(p) % Arena<>::alignment == 0);
}

TEST_CASE("Arena AllocateOrNull returns nullptr for zero bytes") {
  constexpr size_t cap = 128;
  Arena<> arena(cap);

  void *p = arena.AllocateOrNull(0);
  CHECK(p == nullptr);
}

TEST_CASE("Arena AllocateOrNull behaves like Allocate for valid sizes") {
  constexpr size_t cap = 128;
  Arena<> arena(cap);

  void *p = arena.AllocateOrNull(16);
  REQUIRE(p != nullptr);

  CHECK(reinterpret_cast<uintptr_t>(p) % Arena<>::alignment == 0);
}

TEST_CASE("Arena fills memory sequentially without overlap") {
  constexpr size_t cap = 256;
  Arena<> arena(cap);

  void *blocks[8];
  for (int i = 0; i < 8; ++i) {
    blocks[i] = arena.Allocate(16);
    REQUIRE(blocks[i] != nullptr);
  }

  for (int i = 1; i < 8; ++i) {
    CHECK(reinterpret_cast<uintptr_t>(blocks[i]) ==
          reinterpret_cast<uintptr_t>(blocks[i - 1]) + 16);
  }
}

TEST_CASE("Arena memory is writable and stable") {
  constexpr size_t cap = 256;
  Arena<> arena(cap);

  auto *p = static_cast<uint8_t *>(arena.Allocate(64));
  REQUIRE(p != nullptr);

  for (uint8_t i = 0; i < 64; ++i) {
    p[i] = i;
  }

  for (uint8_t i = 0; i < 64; ++i) {
    CHECK(p[i] == i);
  }
}

TEST_CASE("Arena exact stride equals alignment for small allocations") {
  constexpr size_t cap = 128;
  Arena<> arena(cap);

  void *p1 = arena.Allocate(1);
  void *p2 = arena.Allocate(1);
  void *p3 = arena.Allocate(1);

  REQUIRE(p1);
  REQUIRE(p2);
  REQUIRE(p3);

  auto a = Arena<>::alignment;

  CHECK(reinterpret_cast<uintptr_t>(p2) - reinterpret_cast<uintptr_t>(p1) == a);

  CHECK(reinterpret_cast<uintptr_t>(p3) - reinterpret_cast<uintptr_t>(p2) == a);
}

TEST_CASE("Arena rounds up misaligned allocation sizes correctly") {
  constexpr size_t cap = 256;
  Arena<> arena(cap);

  auto a = Arena<>::alignment;

  void *p1 = arena.Allocate(3);  // should round to alignment
  void *p2 = arena.Allocate(5);  // should round to alignment
  void *p3 = arena.Allocate(7);  // should round to alignment

  REQUIRE(p1);
  REQUIRE(p2);
  REQUIRE(p3);

  CHECK(reinterpret_cast<uintptr_t>(p2) - reinterpret_cast<uintptr_t>(p1) == a);
  CHECK(reinterpret_cast<uintptr_t>(p3) - reinterpret_cast<uintptr_t>(p2) == a);
}

TEST_CASE("Arena exact pointer differences with mixed sizes") {
  constexpr size_t cap = 256;
  Arena<> arena(cap);

  auto a = Arena<>::alignment;

  void *p1 = arena.Allocate(8);   // 8 → 8
  void *p2 = arena.Allocate(3);   // 3 → 8
  void *p3 = arena.Allocate(16);  // 16 → 16
  void *p4 = arena.Allocate(5);   // 5 → 8

  REQUIRE(p1);
  REQUIRE(p2);
  REQUIRE(p3);
  REQUIRE(p4);

  CHECK(reinterpret_cast<uintptr_t>(p2) - reinterpret_cast<uintptr_t>(p1) == 8);
  CHECK(reinterpret_cast<uintptr_t>(p3) - reinterpret_cast<uintptr_t>(p2) == a);
  CHECK(reinterpret_cast<uintptr_t>(p4) - reinterpret_cast<uintptr_t>(p3) == 16);
}

TEST_CASE("Arena always returns aligned pointers even for garbage sizes") {
  constexpr size_t cap = 256;
  Arena<> arena(cap);

  uint32_t sizes[] = {1, 2, 3, 5, 7, 9, 15, 31};

  for (uint32_t sz : sizes) {
    void *p = arena.Allocate(sz);
    REQUIRE(p);
    CHECK(reinterpret_cast<uintptr_t>(p) % Arena<>::alignment == 0);
  }
}

TEST_CASE("Arena preserves data across mixed-size allocations") {
  constexpr size_t cap = 4096;
  Arena<> arena(cap);

  struct Block {
    uint8_t *ptr;
    size_t size;
    uint8_t pattern;
  };

  // Hand-randomized, intentionally hostile order
  const size_t sizes[] = {
      64, 3, 512, 1, 31, 128, 7, 1024, 15, 2, 256, 9, 33, 5, 511, 8,
  };

  std::vector<Block> blocks;
  blocks.reserve(std::size(sizes));

  // Allocate + poison memory
  for (size_t i = 0; i < std::size(sizes); ++i) {
    size_t sz = sizes[i];
    uint8_t pattern = static_cast<uint8_t>(0xD0 + i);

    uint8_t *p = static_cast<uint8_t *>(arena.Allocate(sz));
    REQUIRE(p);

    // Alignment must still hold
    CHECK(reinterpret_cast<uintptr_t>(p) % Arena<>::alignment == 0);

    for (size_t j = 0; j < sz; ++j) {
      p[j] = pattern;
    }

    blocks.push_back(Block{p, sz, pattern});
  }

  // Verify nothing got corrupted
  for (const Block &b : blocks) {
    for (size_t j = 0; j < b.size; ++j) {
      CHECK(b.ptr[j] == b.pattern);
    }
  }
}

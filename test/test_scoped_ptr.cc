#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "data/allocator.h"
#include "data/scoped_ptr.h"

using namespace pdp;

TEST_CASE("ScopedPtr default constructed is null and does not leak") {
  TrackingAllocator::Stats stats;
  TrackingAllocator alloc(&stats);

  {
    ScopedPtr<int, TrackingAllocator> p(nullptr, alloc);
    CHECK(!p);
    CHECK(p.Get() == nullptr);
  }

  CHECK(stats.GetAllocationsMade() == 0);
  CHECK(stats.GetDeallocationsMade() == 0);
  CHECK_FALSE(stats.HasLeaks());
}

TEST_CASE("ScopedPtr deallocates owned pointer exactly once") {
  TrackingAllocator::Stats stats;

  TrackingAllocator alloc(&stats);
  int *ptr = static_cast<int *>(alloc.AllocateRaw(sizeof(int)));

  {
    ScopedPtr<int, TrackingAllocator> p(ptr, alloc);
    CHECK(p);
  }

  CHECK(stats.GetAllocationsMade() == 1);
  CHECK(stats.GetDeallocationsMade() == 1);
  CHECK_FALSE(stats.HasLeaks());
}

TEST_CASE("ScopedPtr move constructor transfers ownership") {
  TrackingAllocator::Stats stats;
  TrackingAllocator alloc(&stats);

  int *ptr = static_cast<int *>(alloc.AllocateRaw(sizeof(int)));

  {
    ScopedPtr<int, TrackingAllocator> a(ptr, alloc);
    ScopedPtr<int, TrackingAllocator> b(std::move(a));

    CHECK(!a);
    CHECK(a.Get() == nullptr);

    CHECK(b);
    CHECK(b.Get() == ptr);
  }

  CHECK(stats.GetAllocationsMade() == 1);
  CHECK(stats.GetDeallocationsMade() == 1);
  CHECK_FALSE(stats.HasLeaks());
}

TEST_CASE("ScopedPtr moved-from object does not deallocate") {
  TrackingAllocator::Stats stats;
  TrackingAllocator alloc(&stats);

  int *ptr = static_cast<int *>(alloc.AllocateRaw(sizeof(int)));

  {
    ScopedPtr<int, TrackingAllocator> a(ptr, alloc);
    {
      ScopedPtr<int, TrackingAllocator> b(std::move(a));
    }
    CHECK(stats.GetDeallocationsMade() == 1);
  }

  CHECK(stats.GetAllocationsMade() == 1);
  CHECK(stats.GetDeallocationsMade() == 1);
  CHECK_FALSE(stats.HasLeaks());
}

TEST_CASE("ScopedPtr<void> works correctly") {
  TrackingAllocator::Stats stats;
  TrackingAllocator alloc(&stats);

  void *ptr = alloc.AllocateRaw(16);

  {
    ScopedPtr<void, TrackingAllocator> p(ptr, alloc);
    CHECK(p);
    CHECK(p.Get() == ptr);
  }

  CHECK(stats.GetAllocationsMade() == 1);
  CHECK(stats.GetDeallocationsMade() == 1);
  CHECK_FALSE(stats.HasLeaks());
}

TEST_CASE("ScopedPtr operator-> forwards access") {
  TrackingAllocator::Stats stats;
  TrackingAllocator alloc(&stats);

  struct Pod {
    int x;
  };

  Pod *ptr = static_cast<Pod *>(alloc.AllocateRaw(sizeof(Pod)));
  ptr->x = 42;

  {
    ScopedPtr<Pod, TrackingAllocator> p(ptr, alloc);
    CHECK(p->x == 42);
  }

  CHECK(stats.GetAllocationsMade() == 1);
  CHECK(stats.GetDeallocationsMade() == 1);
  CHECK_FALSE(stats.HasLeaks());
}

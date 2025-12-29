#pragma once

// TODO: idea
// allocator which holds memory and has a threshold for memory usage
// if it rises too much it starts freeing memory.

// TODO: idea
// tagging an Allocator<size_t> and providing statistics for a specific allocation.
// BIG
// BRAIN
// POWER

#include "check.h"

#include <cstddef>
#include <limits>

namespace pdp {

struct BasicAllocator {
  void *AllocateRaw(size_t bytes);
  void DeallocateRaw(void *ptr, size_t bytes);
  void *ReallocateRaw(void *ptr, size_t bytes);
};

struct TracingAllocator {
  void *AllocateRaw(size_t bytes);
  void DeallocateRaw(void *ptr, size_t bytes);
  void *ReallocateRaw(void *ptr, size_t bytes);
};

template <typename T>
void CheckAllocation(size_t n) {
  pdp_assert(n > 0);
  if constexpr (sizeof(T) > 1) {
    const bool no_overflow = std::numeric_limits<size_t>::max() / sizeof(T) >= n;
    pdp_assert(no_overflow);
  }
}

template <typename T, typename Alloc>
T *Allocate(Alloc &allocator, size_t n) {
  CheckAllocation<T>(n);
  return static_cast<T *>(allocator.AllocateRaw(n * sizeof(T)));
}

template <typename T, typename Alloc>
T *Reallocate(Alloc &allocator, T *ptr, size_t n) {
  CheckAllocation<T>(n);
  return static_cast<T *>(allocator.ReallocateRaw(ptr, n * sizeof(T)));
}

template <typename T, typename Alloc>
void Deallocate(Alloc &allocator, void *ptr, size_t size) {
  allocator.DeallocateRaw(ptr, size * sizeof(T));
}

#ifdef PDP_TRACE_ALLOCATIONS
using DefaultAllocator = TracingAllocator;
#else
using DefaultAllocator = BasicAllocator;
#endif

};  // namespace pdp

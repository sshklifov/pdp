#pragma once

// TODO: idea
// allocator which holds memory and has a threshold for memory usage
// if it rises too much it starts freeing memory.

// TODO: idea
// tagging an Allocator<size_t> and providing statistics for a specific allocation.
// BIG
// BRAIN
// POWER

#include "core/check.h"

#include <cstddef>
#include <cstdlib>
#include <limits>

namespace pdp {

struct MallocAllocator {
  void *AllocateRaw(size_t bytes) { return malloc(bytes); }

  void DeallocateRaw(void *ptr) { free(ptr); }

  void *ReallocateRaw(void *ptr, size_t new_bytes) { return realloc(ptr, new_bytes); }
};

namespace impl {
struct _OnceAllocator {
  _OnceAllocator() : entered(false) {}

  void *AllocateRaw(size_t bytes) {
    pdp_assert(!entered);
    entered = true;
    return helper_alloc.AllocateRaw(bytes);
  }

  void DeallocateRaw(void *ptr) { return helper_alloc.DeallocateRaw(ptr); }

  void *ReallocateRaw(void *ptr, size_t new_bytes) {
    pdp_assert(!ptr);
    return realloc(ptr, new_bytes);
  }

 private:
  MallocAllocator helper_alloc;
  bool entered;
};
}  // namespace impl

#ifdef PDP_ENABLE_ASSERT
using OneShotAllocator = impl::_OnceAllocator;
#else
using OneShotAllocator = MallocAllocator;
#endif

template <typename T>
void CheckAllocation(size_t n) {
  pdp_assert(n > 0);
  if constexpr (sizeof(T) > 1) {
    [[maybe_unused]]
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

// TODO horrible practice (looks bad and not readable)...
template <typename T, typename Alloc>
void Deallocate(Alloc &allocator, void *ptr) {
  allocator.DeallocateRaw(ptr);
}

#ifdef PDP_TRACE_ALLOCATIONS
using DefaultAllocator = TracingAllocator;
#else
using DefaultAllocator = MallocAllocator;
#endif

};  // namespace pdp

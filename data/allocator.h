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

#include <malloc.h>

#include <atomic>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <limits>

namespace pdp {

// @brief Bytes (B)
constexpr std::size_t operator""_B(unsigned long long bytes) { return bytes; }

// @brief Kilobytes (KB)
constexpr std::size_t operator""_KB(unsigned long long kilobytes) { return kilobytes * 1024; }

// @brief Megabytes (MB)
constexpr std::size_t operator""_MB(unsigned long long megabytes) {
  return megabytes * 1024 * 1024;
}

// @brief Gigabytes (GB)
constexpr std::size_t operator""_GB(unsigned long long gigabytes) {
  return gigabytes * 1024 * 1024 * 1024;
}

struct AlignmentTraits {
  static constexpr uint32_t AlignUp(uint32_t bytes) {
    return bytes = (bytes + alignment - 1) & ~(alignment - 1);
  }

  static constexpr uint32_t alignment = 8;
};

struct MallocAllocator {
  void *AllocateRaw(size_t bytes) {
    void *ptr = malloc(bytes);
#ifdef PDP_ENABLE_ZERO_INITIALIZE
    memset(ptr, 0, bytes);
#endif
    return ptr;
  }

  void DeallocateRaw(void *ptr) { free(ptr); }

  void *ReallocateRaw(void *ptr, size_t new_bytes) { return realloc(ptr, new_bytes); }

  size_t GetAllocationSize(void *ptr) { return malloc_usable_size(ptr); }
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

struct TrackingAllocator {
  struct Stats {
    friend TrackingAllocator;

    Stats() : bytes_used(0), allocations_made(0), deallocations_made(0) {}

    int64_t GetActiveAllocations() const { return allocations_made - deallocations_made; }

    int64_t GetAllocationsMade() const { return allocations_made; }

    int64_t GetDeallocationsMade() const { return deallocations_made; }

    int64_t GetBytesUsed() const { return bytes_used; }

    bool HasLeaks() const { return bytes_used > 0 || GetActiveAllocations() > 0; }

   private:
    std::atomic_int64_t bytes_used;
    std::atomic_int64_t allocations_made;
    std::atomic_int64_t deallocations_made;
  };

  TrackingAllocator(Stats *st) : stats(st) {}

  void *AllocateRaw(size_t bytes) {
    if (bytes > 0) {
      stats->allocations_made.fetch_add(1);
      void *ptr = helper_alloc.AllocateRaw(bytes);
      stats->bytes_used.fetch_add(helper_alloc.GetAllocationSize(ptr));
      return ptr;
    } else {
      pdp_assert(false);
      return nullptr;
    }
  }

  void DeallocateRaw(void *ptr) {
    if (ptr) {
      stats->bytes_used.fetch_sub(helper_alloc.GetAllocationSize(ptr));
      stats->deallocations_made.fetch_add(1);
    }
    return helper_alloc.DeallocateRaw(ptr);
  }

  void *ReallocateRaw(void *ptr, size_t new_bytes) {
    if (ptr == nullptr && new_bytes > 0) {
      stats->allocations_made.fetch_add(1);
    } else if (ptr != nullptr && new_bytes == 0) {
      stats->deallocations_made.fetch_add(1);
    }
    stats->bytes_used.fetch_sub(helper_alloc.GetAllocationSize(ptr));
    ptr = realloc(ptr, new_bytes);
    stats->bytes_used.fetch_add(helper_alloc.GetAllocationSize(ptr));
    return ptr;
  }

 private:
  Stats *stats;
  MallocAllocator helper_alloc;
};

template <typename T>
void CheckAllocation(size_t n) {
  pdp_assert(n > 0);
  if constexpr (sizeof(T) > 1) {
    [[maybe_unused]]
    const bool no_overflow = std::numeric_limits<size_t>::max() / sizeof(T) >= n;
    pdp_assert(no_overflow);
  }
}

// TODO horrible practice (looks bad and not readable)...

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
void Deallocate(Alloc &allocator, void *ptr) {
  allocator.DeallocateRaw(ptr);
}

#ifdef PDP_TRACE_ALLOCATIONS
using DefaultAllocator = TracingAllocator;
#else
using DefaultAllocator = MallocAllocator;
#endif

};  // namespace pdp

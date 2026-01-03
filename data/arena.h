#pragma once

#include "core/check.h"
#include "data/allocator.h"

#include <cstdint>

namespace pdp {

template <typename Alloc = DefaultAllocator>
struct Arena : public AlignmentTraits {
  Arena(size_t cap) {
    pdp_assert(cap < max_capacity);
    chunk = static_cast<unsigned char *>(allocator.AllocateRaw(cap));

    pdp_assert(chunk);
    pdp_assert(reinterpret_cast<uint64_t>(chunk) % alignment == 0);
    head = chunk;
#ifdef PDP_ENABLE_ASSERT
    capacity = cap;
#endif
  }

  ~Arena() {
    pdp_assert(chunk);
    allocator.DeallocateRaw(chunk);
  }

  void *Allocate(uint32_t bytes) {
    return PDP_ASSUME_ALIGNED(AllocateUnchecked(AlignUp(bytes)), alignment);
  }

  void *AllocateUnchecked(uint32_t bytes) {
    pdp_assert(bytes > 0);
    pdp_assert(bytes % alignment == 0);
    pdp_assert(uint32_t(head - chunk) + bytes <= capacity);
    void *ptr = head;
    head += bytes;
    return PDP_ASSUME_ALIGNED(ptr, alignment);
  }

  void *AllocateOrNull(uint32_t bytes) {
    if (PDP_LIKELY(bytes > 0)) {
      return Allocate(bytes);
    } else {
      return nullptr;
    }
  }

  static constexpr size_t max_capacity = 1_GB;

 private:
  unsigned char *chunk;
  unsigned char *head;
#ifdef PDP_ENABLE_ASSERT
  size_t capacity;
#endif

  Alloc allocator;
};

}  // namespace pdp

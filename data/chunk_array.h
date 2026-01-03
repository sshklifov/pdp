#pragma once

#include "data/allocator.h"
#include "data/stack.h"
#include "tracing/trace_likely.h"

namespace pdp {

template <typename Alloc = DefaultAllocator>
struct ChunkArray : public AlignmentTraits {
  ChunkArray() : chunks(16) {
    chunks += static_cast<unsigned char *>(allocator.AllocateRaw(chunk_size));
    top_used_bytes = 0;
    all_used_bytes = chunk_size;
  }

  ~ChunkArray() {
    for (size_t i = 0; i < chunks.Size(); ++i) {
      allocator.DeallocateRaw(chunks[i]);
    }
  }

  void *Allocate(uint32_t bytes) {
    return PDP_ASSUME_ALIGNED(AllocateUnchecked(AlignUp(bytes)), alignment);
  }

  void *AllocateUnchecked(uint32_t bytes) {
    pdp_assert(bytes > 0);
    pdp_assert(bytes % alignment == 0);

    if (PDP_TRACE_LIKELY(top_used_bytes + bytes <= chunk_size)) {
      top_used_bytes += bytes;
      return chunks.Top() + top_used_bytes;
    }

    if (PDP_TRACE_UNLIKELY(bytes >= chunk_size)) {
      pdp_assert(all_used_bytes <= max_capacity - bytes);
      unsigned char *result = static_cast<unsigned char *>(allocator.AllocateRaw(bytes));
      all_used_bytes += bytes;

      auto top_chunk = chunks.Top();
      chunks.Top() = result;
      chunks += top_chunk;
      return result;
    }

    pdp_assert(all_used_bytes <= max_capacity - chunk_size);
    unsigned char *result = static_cast<unsigned char *>(allocator.AllocateRaw(chunk_size));
    all_used_bytes += chunk_size;
    chunks += result;
    top_used_bytes = bytes;
    return result;
  }

  void *AllocateOrNull(uint32_t bytes) {
    if (PDP_LIKELY(bytes > 0)) {
      return Allocate(bytes);
    } else {
      return nullptr;
    }
  }

  size_t NumChunks() { return chunks.Size(); }

  static constexpr size_t chunk_size = 64_KB;
  static constexpr size_t max_capacity = 1_GB;

 private:
  size_t top_used_bytes;
#ifdef PDP_ENABLE_ASSERT
  size_t all_used_bytes;
#endif
  Stack<unsigned char *> chunks;

  Alloc allocator;
};

}  // namespace pdp

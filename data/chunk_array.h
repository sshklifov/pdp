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
#ifdef PDP_TRACE_CHUNK_ARRAY
    all_used_bytes = chunk_size;
#endif
  }

  ~ChunkArray() {
#ifdef PDP_TRACE_CHUNK_ARRAY
    pdp_trace("Chunk array requested {}B vs actually allocated {}B", requested_bytes,
              allocated_bytes);
    pdp_trace("Total {} calls to malloc", chunks.Size());
#endif
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
#ifdef PDP_TRACE_CHUNK_ARRAY
    requested_bytes += bytes;
#endif

    if (PDP_TRACE_LIKELY(top_used_bytes + bytes <= chunk_size)) {
      void *ret = chunks.Top() + top_used_bytes;
      top_used_bytes += bytes;
      return ret;
    }

    if (PDP_TRACE_UNLIKELY(bytes >= chunk_size)) {
#ifdef PDP_TRACE_CHUNK_ARRAY
      pdp_assert(all_used_bytes <= max_capacity - bytes);
      allocated_bytes += bytes;
#endif
      unsigned char *big_chunk = static_cast<unsigned char *>(allocator.AllocateRaw(bytes));

      auto normal_chunk = chunks.Top();
      chunks.Top() = big_chunk;
      chunks += normal_chunk;
      return big_chunk;
    }

#ifdef PDP_TRACE_CHUNK_ARRAY
    pdp_assert(all_used_bytes <= max_capacity - chunk_size);
    allocated_bytes += chunk_size;
#endif
    unsigned char *result = static_cast<unsigned char *>(allocator.AllocateRaw(chunk_size));
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
#ifdef PDP_TRACE_CHUNK_ARRAY
  size_t allocated_bytes;
  size_t requested_bytes;
#endif
  Stack<unsigned char *> chunks;

  Alloc allocator;
};

}  // namespace pdp

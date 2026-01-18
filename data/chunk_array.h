#pragma once

// TODO flagged as not required

#include "data/allocator.h"
#include "data/stack.h"

namespace pdp {

struct ChunkHandle : public NonCopyable {
  ChunkHandle(byte **chunks, size_t num_chunks);
  ChunkHandle(ChunkHandle &&rhs);
  void operator=(ChunkHandle &&rhs) = delete;
  ~ChunkHandle();

 private:
  byte **chunks;
  size_t num_chunks;
};

struct ChunkArray : public AlignmentTraits, public NonCopyableNonMovable {
  ChunkArray();
  ~ChunkArray();

  void *Allocate(uint32_t bytes);
  void *AllocateUnchecked(uint32_t bytes);
  void *AllocateOrNull(uint32_t bytes);

  size_t NumChunks();

  [[nodiscard]] ChunkHandle ReleaseChunks();

  static constexpr size_t chunk_size = 64_KB;
  static constexpr size_t max_capacity = 1_GB;

 private:
  size_t top_used_bytes;
#ifdef PDP_TRACE_CHUNK_ARRAY
  size_t allocated_bytes;
  size_t requested_bytes;
#endif
  Stack<byte *> chunks;

  DefaultAllocator allocator;
};

}  // namespace pdp

#pragma once

#include "data/allocator.h"
#include "data/stack.h"

// TODO: optimize be knowing the structure of returned messages, so you can instead do something
// like:
// struct Response{ int msgid, struct {int buffer_id, int ... } }
// you know? and not use an allocator

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

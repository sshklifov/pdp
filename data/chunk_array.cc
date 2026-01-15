#include "chunk_array.h"
#include "tracing/trace_likely.h"

namespace pdp {

namespace impl {

template <>
struct _StackPrivAccess<DefaultAllocator> {
  _StackPrivAccess(Stack<byte *> &s) : s(s) {}

  [[nodiscard]] ChunkHandle ReleaseChunks() {
    ChunkHandle handle(s.ptr, s.size);
    pdp_assert(s.ptr);
    s.ptr = nullptr;
    return handle;
  }

  bool IsHoldingChunks() const { return s.ptr != nullptr; }

 private:
  Stack<byte *> &s;
};

}  // namespace impl

ChunkHandle::ChunkHandle(byte **chunks, size_t num_chunks)
    : chunks(chunks), num_chunks(num_chunks) {}

ChunkHandle::ChunkHandle(ChunkHandle &&rhs) : chunks(rhs.chunks), num_chunks(rhs.num_chunks) {
  rhs.chunks = nullptr;
}

ChunkHandle::~ChunkHandle() {
  if (chunks) {
    for (size_t i = 0; i < num_chunks; ++i) {
      free(chunks[i]);
    }
    free(chunks);
  }
}

ChunkHandle ChunkArray::ReleaseChunks() {
  impl::_StackPrivAccess<DefaultAllocator> _stack_priv(chunks);
  return _stack_priv.ReleaseChunks();
}

ChunkArray::ChunkArray() : chunks(16) {
  chunks += static_cast<byte *>(allocator.AllocateRaw(chunk_size));
  top_used_bytes = 0;
#ifdef PDP_TRACE_CHUNK_ARRAY
  all_used_bytes = chunk_size;
#endif
}

ChunkArray::~ChunkArray() {
#ifdef PDP_TRACE_CHUNK_ARRAY
  pdp_trace("Chunk array requested {}B vs actually allocated {}B", requested_bytes,
            allocated_bytes);
  pdp_trace("Total {} calls to malloc", chunks.Size());
#endif
  impl::_StackPrivAccess<DefaultAllocator> _stack_priv(chunks);
  if (_stack_priv.IsHoldingChunks()) {
    for (size_t i = 0; i < chunks.Size(); ++i) {
      allocator.DeallocateRaw(chunks[i]);
    }
  }
}

void *ChunkArray::Allocate(uint32_t bytes) {
  return PDP_ASSUME_ALIGNED(AllocateUnchecked(AlignUp(bytes)), alignment);
}

void *ChunkArray::AllocateUnchecked(uint32_t bytes) {
  pdp_assert(chunks.Data());
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
    byte *big_chunk = static_cast<byte *>(allocator.AllocateRaw(bytes));

    auto normal_chunk = chunks.Top();
    chunks.Top() = big_chunk;
    chunks += normal_chunk;
    return big_chunk;
  }

#ifdef PDP_TRACE_CHUNK_ARRAY
  pdp_assert(all_used_bytes <= max_capacity - chunk_size);
  allocated_bytes += chunk_size;
#endif
  byte *result = static_cast<byte *>(allocator.AllocateRaw(chunk_size));
  chunks += result;
  top_used_bytes = bytes;
  return result;
}

void *ChunkArray::AllocateOrNull(uint32_t bytes) {
  if (PDP_LIKELY(bytes > 0)) {
    return Allocate(bytes);
  } else {
    return nullptr;
  }
}

size_t ChunkArray::NumChunks() { return chunks.Size(); }

}  // namespace pdp

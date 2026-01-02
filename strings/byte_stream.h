// TODO pointless?

#pragma once

#include "string_slice.h"
#include "tracing/tracing_counter.h"

namespace pdp {

struct ByteStream {
  static constexpr const size_t in_place_threshold = 4_KB;
  static constexpr const size_t default_buffer_size = 1_MB;
  static constexpr const size_t max_capacity = 1_GB;

  ByteStream();
  ~ByteStream();

  uint8_t PopByte();

  uint8_t PopUint8();
  int8_t PopInt8();

  uint16_t PopUint16();
  int16_t PopInt16();

  uint32_t PopUint32();
  int32_t PopInt32();

  uint64_t PopUint64();
  int64_t PopInt64();

  void Memcpy(void *dst, size_t n);

 private:
  void RequireAtLeast(size_t n);
  void FetchNew(size_t n);
  // void ReserveForRead();

  char *__restrict__ ptr;
  char *__restrict__ begin;
  char *__restrict__ end;

  DefaultAllocator allocator;

#ifdef PDP_TRACE_ROLLING_BUFFER
  enum Counters { kEmptyOptimization, kMinSize, kMoved, kAllocation, kTotal };
  static constexpr std::array<const char *, kTotal> names{"Empty optimization", "Have min size",
                                                          "Moved", "Allocation"};
  TracingCounter<kTotal> provide_bytes;
#endif
};

}  // namespace pdp

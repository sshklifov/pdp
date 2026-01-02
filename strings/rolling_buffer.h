#pragma once

#include "string_slice.h"
#include "tracing/tracing_counter.h"

namespace pdp {

struct RollingBuffer {
  static constexpr const size_t min_read_size = 4_KB;
  static constexpr const size_t default_buffer_size = 1_MB;
  static constexpr const size_t max_capacity = 1_GB;

  RollingBuffer();
  ~RollingBuffer();

  StringSlice ReadLine(int fd);

 private:
  void ReserveForRead();

  char *ptr;
  char *begin;
  char *end;
  char *limit;

  DefaultAllocator allocator;

#ifdef PDP_TRACE_ROLLING_BUFFER
  enum Counters { kEmptyOptimization, kMinSize, kMoved, kAllocation, kTotal };
  static constexpr std::array<const char *, kTotal> names{"Empty optimization", "Have min size",
                                                          "Moved", "Allocation"};
  TracingCounter<kTotal> provide_bytes;
#endif
};

}  // namespace pdp

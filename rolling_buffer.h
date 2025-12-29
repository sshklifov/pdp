#pragma once

#include "string_slice.h"
#include "tracing_counter.h"

namespace pdp {

struct RollingBuffer {
  static constexpr const size_t min_read_size = 4'000;
  static constexpr const size_t default_buffer_size = min_read_size * 2;
  static constexpr const size_t max_capacity = 1 << 30;

  RollingBuffer();
  ~RollingBuffer();

  size_t ReadFull(int fd);

  StringSlice ConsumeLine();

  StringSlice ViewOnly() const;

  bool Empty() const;
  size_t Size() const;

 private:
  void ReserveForRead();

  char *ptr;
  char *begin;
  char *end;
  size_t capacity;

  DefaultAllocator allocator;

#ifdef PDP_TRACE_ROLLING_BUFFER
  enum Counters { kEmptyOptimization, kMinSize, kMoved, kAllocation, kTotal };
  static constexpr std::array<const char *, kTotal> names{"Empty optimization", "Have min size",
                                                          "Moved", "Allocation"};
  TracingCounter<kTotal> provide_bytes;
#endif
};

}  // namespace pdp

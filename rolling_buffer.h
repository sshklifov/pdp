#pragma once

#include "string_slice.h"
#include "tracing_counter.h"

namespace pdp {

struct RollingBuffer {
  static constexpr const size_t min_read_size = 4'000;
  static constexpr const size_t default_buffer_size = min_read_size * 2;

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
  size_t begin;
  size_t end;
  size_t capacity;

  enum Counters { kEmptyOptimization, kNotNeeded, kMoved, kAllocation, kTotal };
  static constexpr std::array<const char *, kTotal> names{"Empty optimization", "Not needed",
                                                          "Moved", "Allocation"};
  TracingCounter<kTotal> provide_bytes;
};

}  // namespace pdp

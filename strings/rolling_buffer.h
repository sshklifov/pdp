#pragma once

#include "system/file_descriptor.h"
#include "tracing/tracing_counter.h"

namespace pdp {

struct MutableLine {
  MutableLine() : begin(nullptr), end(nullptr) {}
  MutableLine(char *b, char *e) : begin(b), end(e) {}

  char *begin;
  char *end;
};

struct RollingBuffer {
  static constexpr size_t min_read_size = 4_KB;
  static constexpr size_t default_buffer_size = 16_KB;
  static constexpr size_t max_capacity = 512_MB;

  RollingBuffer();
  ~RollingBuffer();

  void SetDescriptor(int fd);
  int GetDescriptor() const;

  MutableLine ReadLine();

 private:
  void ReserveForRead();

  char *__restrict__ ptr;
  char *__restrict__ begin;
  char *__restrict__ end;
  const char *__restrict__ limit;

  bool search_for_newlines;
  InputDescriptor input_fd;
  DefaultAllocator allocator;

#ifdef PDP_TRACE_ROLLING_BUFFER
  enum Counters { kEmptyOptimization, kMinSize, kMoved, kAllocation, kTotal };
  static constexpr std::array<const char *, kTotal> names{"Empty optimization", "Have min size",
                                                          "Moved", "Allocation"};
  TracingCounter<kTotal> provide_bytes;
#endif
};

}  // namespace pdp

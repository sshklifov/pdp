#include "rolling_buffer.h"
#include "core/check.h"
#include "tracing/trace_likely.h"

#include <unistd.h>
#include <cerrno>
#include <cstdlib>
#include <cstring>

namespace pdp {

RollingBuffer::RollingBuffer()
#ifdef PDP_TRACE_ROLLING_BUFFER
    : capacity(default_buffer_size),
      provide_bytes(names)
#endif
{
  ptr = Allocate<char>(allocator, default_buffer_size);
  pdp_assert(ptr);
  begin = ptr;
  end = ptr;
  limit = begin + default_buffer_size;
}

RollingBuffer::~RollingBuffer() { Deallocate<char>(allocator, ptr); }

StringSlice RollingBuffer::ReadLine(int fd) {
  if (PDP_TRACE_UNLIKELY(begin != end)) {
    char *pos = static_cast<char *>(memchr(begin, '\n', end - begin));
    if (pos) {
      ++pos;
      StringSlice res(begin, pos);
      begin = pos;
      return res;
    }
  }

  for (;;) {
    ReserveForRead();
    const size_t remaining_bytes = limit - end;
    pdp_assert(remaining_bytes >= min_read_size);
    ssize_t ret = read(fd, end, remaining_bytes);

    if (ret <= 0) {
      if (PDP_UNLIKELY(errno != EAGAIN && errno != EWOULDBLOCK)) {
        Check(ret, "read");
      }
      return StringSlice(begin, begin);
    }

    char *pos = static_cast<char *>(memchr(end, '\n', ret));
    end += ret;
    pdp_assert(end <= limit);
    if (pos) {
      ++pos;
      StringSlice res(begin, pos);
      begin = pos;
      return res;
    }
  }
}

void RollingBuffer::ReserveForRead() {
  const bool empty = (begin == end);
  if (PDP_LIKELY(empty)) {
    begin = ptr;
    end = ptr;
#ifdef PDP_TRACE_ROLLING_BUFFER
    provide_bytes.Count(kEmptyOptimization);
#endif
    return;
  }

  const size_t curr_free = limit - end;
  if (PDP_LIKELY(curr_free >= min_read_size)) {
#ifdef PDP_TRACE_ROLLING_BUFFER
    provide_bytes.Count(kMinSize);
#endif
    return;
  }

  const size_t fragmented_size = begin - ptr;
  const size_t used_size = end - begin;
  if (PDP_UNLIKELY(fragmented_size >= used_size)) {
    memcpy(ptr, begin, used_size);
    begin -= fragmented_size;
    end -= fragmented_size;
#ifdef PDP_TRACE_ROLLING_BUFFER
    provide_bytes.Count(kMoved);
#endif
    return;
  }

  size_t capacity = limit - ptr;
  size_t grow_capacity = capacity / 2;
  [[maybe_unused]]
  const bool within_limits = max_capacity - grow_capacity >= capacity;
  pdp_assert(within_limits);

  capacity += grow_capacity;
  char *new_ptr = Allocate<char>(allocator, capacity);
  pdp_assert(new_ptr);
  memcpy(new_ptr, begin, used_size);
  Deallocate<char>(allocator, ptr);

  ptr = new_ptr;
  begin = new_ptr;
  end = new_ptr + used_size;
  limit = new_ptr + capacity;
#ifdef PDP_TRACE_ROLLING_BUFFER
  provide_bytes.Count(kAllocation);
#endif
}

}  // namespace pdp

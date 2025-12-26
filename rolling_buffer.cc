#include "rolling_buffer.h"
#include "check.h"

#include <unistd.h>
#include <cerrno>
#include <cstdlib>
#include <cstring>

namespace pdp {

RollingBuffer::RollingBuffer()
#ifdef PDP_TRACE_ROLLING_BUFFER
    : begin(0), end(0), capacity(default_buffer_size), provide_bytes(names) {
#else
    : begin(0), end(0), capacity(default_buffer_size) {
#endif
  ptr = Allocate<char>(allocator, default_buffer_size);
}

RollingBuffer::~RollingBuffer() { Deallocate<char>(allocator, ptr, capacity); }

size_t RollingBuffer::ReadFull(int fd) {
  size_t old_size = Size();
  for (;;) {
    ReserveForRead();
    pdp_assert(capacity - end >= min_read_size);
    ssize_t ret = read(fd, ptr + end, capacity - end);

    if (ret <= 0) {
      if (errno != EAGAIN && errno != EWOULDBLOCK) {
        Check(ret, "read");
      }
      pdp_assert(Size() > old_size);
      return Size() - old_size;
    }

    end += static_cast<size_t>(ret);
    pdp_assert(end <= capacity);
  }
}

StringSlice RollingBuffer::ConsumeLine() {
  size_t pos = begin;
  while (pos < end) {
    if (ptr[pos] == '\n') {
      StringSlice res(ptr + begin, ptr + pos + 1);
      begin = pos + 1;
      return res;
    }
    ++pos;
  }
  return StringSlice(ptr, ptr);
}

StringSlice RollingBuffer::ViewOnly() const { return StringSlice(ptr + begin, ptr + end); }

bool RollingBuffer::Empty() const { return begin == end; }

size_t RollingBuffer::Size() const {
  pdp_assert(end >= begin);
  return end - begin;
}

void RollingBuffer::ReserveForRead() {
  const bool empty = Empty();
  if (empty) {
    begin = 0;
    end = 0;
#ifdef PDP_ROLLING_BUFFER
    provide_bytes.Count(kEmptyOptimization);
#endif
    return;
  }

  const size_t curr_free = capacity - end;
  if (curr_free >= min_read_size) {
#ifdef PDP_ROLLING_BUFFER
    provide_bytes.Count(kNotNeeded);
#endif
    return;
  }

  const size_t fragmented_size = begin;
  const size_t used_size = Size();
  if (fragmented_size >= used_size) {
    memcpy(ptr, ptr + begin, used_size);
    begin = 0;
    end = used_size;
#ifdef PDP_ROLLING_BUFFER
    provide_bytes.Count(kMoved);
#endif
    return;
  }

  size_t grow_capacity = capacity / 2;
  const bool within_limits = max_capacity - grow_capacity >= capacity;
  pdp_assert(within_limits);

  char *new_ptr = Allocate<char>(allocator, capacity + grow_capacity);
  pdp_assert(new_ptr);
  memcpy(new_ptr, ptr + begin, used_size);
  Deallocate<char>(allocator, ptr, capacity);

  ptr = new_ptr;
  begin = 0;
  end = used_size;
  capacity += grow_capacity;
#ifdef PDP_ROLLING_BUFFER
  provide_bytes.Count(kAllocation);
#endif
}

}  // namespace pdp

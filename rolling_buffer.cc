#include "rolling_buffer.h"
#include "check.h"
#include "likely.h"

#include <unistd.h>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <limits>

namespace pdp {

RollingBuffer::RollingBuffer()
    : ptr(static_cast<char *>(malloc(default_buffer_size))),
      begin(0),
      end(0),
      capacity(default_buffer_size),
      provide_bytes(names) {}

RollingBuffer::~RollingBuffer() { free(ptr); }

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
    provide_bytes.Count(kEmptyOptimization);
    return;
  }

  const size_t curr_free = capacity - end;
  if (curr_free >= min_read_size) {
    provide_bytes.Count(kNotNeeded);
    return;
  }

  const size_t fragmented_size = begin;
  const size_t used_size = Size();
  if (fragmented_size >= used_size) {
    memcpy(ptr, ptr + begin, used_size);
    begin = 0;
    end = used_size;
    provide_bytes.Count(kMoved);
    return;
  }

  size_t grow_capacity = capacity / 2;
  const size_t max_capacity = std::numeric_limits<size_t>::max();
  bool no_overflow = max_capacity - grow_capacity >= capacity;
  if (PDP_LIKELY(no_overflow)) {
    capacity += grow_capacity;
    char *new_ptr = static_cast<char *>(malloc(capacity));
    pdp_assert(new_ptr);
    memcpy(new_ptr, ptr + begin, used_size);
    free(ptr);
    ptr = new_ptr;
    begin = 0;
    end = used_size;
    provide_bytes.Count(kAllocation);
  } else {
    std::terminate();
  }
}

}  // namespace pdp

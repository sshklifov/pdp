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
    : capacity(default_buffer_size) {
#endif
  ptr = Allocate<char>(allocator, default_buffer_size);
  pdp_assert(ptr);
  begin = ptr;
  end = ptr;
}

RollingBuffer::~RollingBuffer() { Deallocate<char>(allocator, ptr, capacity); }

// TODO don't read full if we found a new line
// keep position of last found new line. and also invalidate it if we reallocate.
size_t RollingBuffer::ReadFull(int fd) {
  size_t old_size = Size();
  for (;;) {
    ReserveForRead();
    const size_t remaining_bytes = (ptr + capacity) - end;
    pdp_assert(remaining_bytes >= min_read_size);
    ssize_t ret = read(fd, end, remaining_bytes);

    if (ret <= 0) {
      if (errno != EAGAIN && errno != EWOULDBLOCK) {
        Check(ret, "read");
      }
      pdp_assert(Size() > old_size);
      return Size() - old_size;
    }

    end += static_cast<size_t>(ret);
    pdp_assert(end <= ptr + capacity);
  }
}

StringSlice RollingBuffer::ConsumeLine() {
  char *it = static_cast<char *>(memchr(begin, '\n', Size()));
  if (it) {
    StringSlice res(begin, it + 1);
    begin = it + 1;
    return res;
  }
  return StringSlice(end, end);
}

// TODO needed?
StringSlice RollingBuffer::ViewOnly() const { return StringSlice(begin, end); }

bool RollingBuffer::Empty() const { return begin == end; }

size_t RollingBuffer::Size() const {
  pdp_assert(end >= begin);
  return end - begin;
}

// TODO careful; not to overwrite with MT!

void RollingBuffer::ReserveForRead() {
  const bool empty = Empty();
  if (empty) {
    begin = ptr;
    end = ptr;
#ifdef PDP_ROLLING_BUFFER
    provide_bytes.Count(kEmptyOptimization);
#endif
    return;
  }

  // TODO min read size eeeh i need some way to give write() more space and defragment

  const size_t curr_free = (ptr + capacity) - end;
  if (curr_free >= min_read_size) {
#ifdef PDP_ROLLING_BUFFER
    provide_bytes.Count(kMinSize);
#endif
    return;
  }

  const size_t fragmented_size = begin - ptr;
  const size_t used_size = end - begin;
  if (fragmented_size >= used_size) {
    memcpy(ptr, begin, used_size);
    begin = ptr;
    end = ptr + used_size;
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
  memcpy(new_ptr, begin, used_size);
  Deallocate<char>(allocator, ptr, capacity);

  ptr = new_ptr;
  begin = new_ptr;
  end = new_ptr + used_size;
  capacity += grow_capacity;
#ifdef PDP_ROLLING_BUFFER
  provide_bytes.Count(kAllocation);
#endif
}

}  // namespace pdp

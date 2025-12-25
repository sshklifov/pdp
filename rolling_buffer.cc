#include "rolling_buffer.h"
#include "check.h"

#include <unistd.h>
#include <cassert>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <limits>

namespace pdp {

RollingBuffer::RollingBuffer()
    : ptr(static_cast<char *>(malloc(default_buffer_capacity))),
      num_skipped(0),
      num_read(0),
      num_free(default_buffer_capacity) {}

RollingBuffer::~RollingBuffer() { free(ptr - num_skipped); }

size_t RollingBuffer::ReadFull(int fd) {
  size_t old_size = num_read;
  while (ReadOnce(fd) > 0);

  assert(num_read >= old_size);
  return num_read - old_size;
}

size_t RollingBuffer::ReadOnce(int fd) {
  Relocate();
  ssize_t ret = read(fd, ptr + num_read, num_free);

  if (ret <= 0) {
    if (errno != EAGAIN && errno != EWOULDBLOCK) {
      Check(ret, "read");
    }
    return 0;
  }

  size_t n = static_cast<size_t>(ret);
  num_read += n;
  assert(n < num_free);
  num_free -= n;
  return n;
}

StringView RollingBuffer::ConsumeLine() {
  size_t pos = 0;
  while (pos < num_read) {
    if (ptr[pos] == '\n') {
      return ConsumeChars(pos + 1);
    }
    ++pos;
  }
  return StringView(ptr, ptr);
}

StringView RollingBuffer::ViewOnly() const { return StringView(ptr, ptr + num_read); }

bool RollingBuffer::Empty() const { return num_read == 0; }

void RollingBuffer::Relocate() {
  const bool empty = Empty();
  if (__builtin_expect(empty, true)) {
    num_free += num_skipped;
    ptr -= num_skipped;
    num_skipped = 0;
    return;
  }

  // TODO change (re)allocation strategy
  if (num_free < 4096) {
    GrowExtra();
    assert(num_free > 4096);
  }
}

StringView RollingBuffer::ConsumeChars(size_t n) {
  assert(n <= num_read);

  StringView res(ptr, n);
  num_skipped += n;
  ptr += n;
  num_read -= n;
  return res;
}

void RollingBuffer::GrowExtra() {
  size_t capacity = num_skipped + num_read + num_free;
  size_t grow_capacity = capacity / 2;
  const size_t max_capacity = std::numeric_limits<size_t>::max();
  bool no_overflow = max_capacity - grow_capacity >= capacity;
  if (__builtin_expect(no_overflow, true)) {
    capacity += grow_capacity;
    char *new_ptr = static_cast<char *>(malloc(capacity));
    assert(new_ptr);
    memcpy(new_ptr, ptr + num_skipped, num_read);
    free(ptr);
    ptr = new_ptr;
    num_skipped = 0;
    assert(capacity > num_read);
    num_free = capacity - num_read;
  } else {
    std::terminate();
  }
}

}  // namespace pdp

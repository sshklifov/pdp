#include "byte_stream.h"
#include "core/check.h"

#include <fcntl.h>
#include <unistd.h>

namespace pdp {

static bool ReadAtLeast(void *buf, size_t min_size, size_t capacity) {
  constexpr unsigned 
}

ByteStream::ByteStream()
#ifdef PDP_TRACE_ROLLING_BUFFER
    : capacity(default_buffer_size),
      provide_bytes(names)
#endif
{
  ptr = Allocate<char>(allocator, default_buffer_size);
  pdp_assert(ptr);
  begin = ptr;
  end = ptr;

  int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
  int ret = fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK);
  CheckAndTerminate(ret, "fcntl");
}

ByteStream::~ByteStream() { Deallocate<char>(allocator, ptr); }

uint8_t ByteStream::PopByte() {
  RequireAtLeast(1);
  uint8_t res = *begin;
  ++begin;
  return res;
}

uint8_t ByteStream::PopUint8() { return PopByte(); }

int8_t ByteStream::PopInt8() { return static_cast<int8_t>(PopByte()); }

uint16_t ByteStream::PopUint16() {
  RequireAtLeast(2);
  uint16_t res = (uint16_t(begin[0]) << 8) | (uint16_t(begin[1]));
  begin += 2;
  return res;
}

int16_t ByteStream::PopInt16() { return static_cast<int16_t>(PopUint16()); }

uint32_t ByteStream::PopUint32() {
  RequireAtLeast(4);
  uint32_t res = (uint32_t(begin[0]) << 24) | (uint32_t(begin[1]) << 16) |
                 (uint32_t(begin[2]) << 8) | (uint32_t(begin[3]));
  begin += 4;
  return res;
}

int32_t ByteStream::PopInt32() { return static_cast<int32_t>(PopUint32()); }

uint64_t ByteStream::PopUint64() {
  RequireAtLeast(8);
  uint64_t res = (uint64_t(begin[0]) << 56) | (uint64_t(begin[1]) << 48) |
                 (uint64_t(begin[2]) << 40) | (uint64_t(begin[3]) << 32) |
                 (uint64_t(begin[4]) << 24) | (uint64_t(begin[5]) << 16) |
                 (uint64_t(begin[6]) << 8) | (uint64_t(begin[7]));
  begin += 8;
  return res;
}

int64_t ByteStream::PopInt64() { return static_cast<int64_t>(PopUint64()); }

void ByteStream::Memcpy(void *dst, size_t n) {
  size_t available = end - begin;
  if (n <= available) {
    memcpy(dst, begin, n);
    begin += n;
    return;
  }

  memcpy(dst, begin, available);
  begin = ptr;
  end = ptr;
  n -= available;
  if (PDP_LIKELY(n < in_place_threshold)) {
    // FetchNew();
    // TODO here
  } else {
    // TODO poll and write... exact bytes
  }
}

void ByteStream::RequireAtLeast(size_t n) {
  size_t available = end - begin;
  if (PDP_UNLIKELY(available < n)) {
    // memcpy(ptr, begin, used_size);
    // begin -= fragmented_size;
    // end -= fragmented_size;
  }
}

void ByteStream::FetchNew(size_t n) {
  // memcpy(ptr, begin, used_size);
  // begin -= fragmented_size;
  // end -= fragmented_size;
}

#if 0
uint8_t ByteStream::PopByte() {
  if (PDP_TRACE_LIKELY(begin < end)) {
    uint8_t res = *begin;
    ++begin;
    return res;
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
#endif

#if 0
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
#endif

}  // namespace pdp

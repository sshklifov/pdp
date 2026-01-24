#include "byte_stream.h"
#include "core/check.h"
#include "core/log.h"

#include <fcntl.h>
#include <unistd.h>

namespace pdp {

ByteStream::ByteStream(int fd) : ptr(Allocate<byte>(allocator, buffer_size)), stream(fd) {
  pdp_assert(ptr);
  begin = ptr;
  end = ptr;
}

ByteStream::~ByteStream() { Deallocate<byte>(allocator, ptr); }

int ByteStream::GetDescriptor() const { return stream.GetDescriptor(); }

bool ByteStream::PollBytes() {
  if (PDP_LIKELY(begin < end)) {
    return true;
  }

  size_t num_read = stream.ReadOnce(ptr, buffer_size);
  begin = ptr;
  end = ptr + num_read;
  return num_read > 0;
}

uint8_t ByteStream::PeekByte() {
  RequireAtLeast(1);
  return *begin;
}

uint8_t ByteStream::PopByte() {
  uint8_t res = PeekByte();
  ++begin;
  return res;
}

uint8_t ByteStream::PopUint8() { return PopByte(); }

int8_t ByteStream::PopInt8() { return BitCast<int8_t>(PopByte()); }

uint16_t ByteStream::PopUint16() {
  RequireAtLeast(2);
  uint16_t res = (uint16_t(begin[0]) << 8) | (uint16_t(begin[1]));
  begin += 2;
  return res;
}

int16_t ByteStream::PopInt16() { return BitCast<int16_t>(PopUint16()); }

uint32_t ByteStream::PopUint32() {
  RequireAtLeast(4);
  uint32_t res = (uint32_t(begin[0]) << 24) | (uint32_t(begin[1]) << 16) |
                 (uint32_t(begin[2]) << 8) | (uint32_t(begin[3]));
  begin += 4;
  return res;
}

int32_t ByteStream::PopInt32() { return BitCast<int32_t>(PopUint32()); }

uint64_t ByteStream::PopUint64() {
  RequireAtLeast(8);
  uint64_t res = (uint64_t(begin[0]) << 56) | (uint64_t(begin[1]) << 48) |
                 (uint64_t(begin[2]) << 40) | (uint64_t(begin[3]) << 32) |
                 (uint64_t(begin[4]) << 24) | (uint64_t(begin[5]) << 16) |
                 (uint64_t(begin[6]) << 8) | (uint64_t(begin[7]));
  begin += 8;
  return res;
}

int64_t ByteStream::PopInt64() { return BitCast<int64_t>(PopUint64()); }

void ByteStream::Memcpy(void *dst, size_t n) {
  size_t available = end - begin;
  if (PDP_LIKELY(n <= available)) {
    memcpy(dst, begin, n);
    begin += n;
    return;
  }

  memcpy(dst, begin, available);
  begin = ptr;
  end = ptr;
  dst = static_cast<byte *>(dst) + available;
  n -= available;
  if (PDP_LIKELY(n < in_place_threshold)) {
    size_t num_read = stream.ReadAtLeast(ptr, n, buffer_size, max_wait);
    if (PDP_UNLIKELY(num_read < n)) {
      pdp_critical("Failed to read {} bytes within {}ms", n, max_wait.Get());
      PDP_UNREACHABLE("RPC stream timeout");
    }
    memcpy(dst, begin, n);
    begin += n;
    end += num_read;
  } else {
    bool success = stream.ReadExactly(dst, n, max_wait);
    if (PDP_UNLIKELY(!success)) {
      pdp_critical("Failed to read {} bytes within {}ms", n, max_wait.Get());
      PDP_UNREACHABLE("RPC stream timeout");
    }
  }
}

void ByteStream::Skip(size_t num_skipped) {
  size_t available = end - begin;
  if (PDP_LIKELY(num_skipped <= available)) {
    begin += num_skipped;
    return;
  }

  num_skipped -= available;

  Milliseconds next_wait = max_wait;
  Stopwatch stopwatch;

  while (num_skipped > 0 && next_wait > 0_ms) {
    stream.WaitForInput(next_wait > 5_ms ? next_wait : 5_ms);
    size_t num_read = 0;
    do {
      num_skipped -= num_read;
      num_read = stream.ReadOnce(ptr, buffer_size);
    } while (num_read > 0 && num_read < num_skipped);

    if (PDP_LIKELY(num_read >= num_skipped)) {
      begin = ptr + num_skipped;
      end = ptr + num_read;
      return;
    }
    next_wait = max_wait - stopwatch.Elapsed();
  }
  pdp_critical("Bytes remaining to skip: {}", num_skipped);
  PDP_UNREACHABLE("RPC stream timeout");
}

void ByteStream::RequireAtLeast(size_t n) {
  size_t available = end - begin;
  if (PDP_UNLIKELY(available < n)) {
    [[maybe_unused]]
    size_t left_padding = begin - ptr;
    size_t size = end - begin;
    pdp_assert(left_padding >= size);
    memcpy(ptr, begin, size);
    begin = ptr;
    end = ptr + size;
    size_t num_read = stream.ReadAtLeast(end, n, buffer_size - n, max_wait);
    if (PDP_UNLIKELY(num_read < n)) {
      pdp_critical("Failed to read {} bytes within {}ms", n, max_wait.Get());
      PDP_UNREACHABLE("RPC stream timeout");
    }

    end += num_read;
  }
}

}  // namespace pdp

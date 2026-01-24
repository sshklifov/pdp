#pragma once

#include "data/allocator.h"
#include "system/file_descriptor.h"

#include <unistd.h>
#include <cstdint>

namespace pdp {

struct ByteStream {
  static constexpr size_t in_place_threshold = 4_KB;
  static constexpr size_t buffer_size = 32_KB;
  static constexpr Milliseconds max_wait = 5000_ms;

  ByteStream(int fd);
  ~ByteStream();

  int GetDescriptor() const;

  bool PollBytes();

  uint8_t PeekByte();
  uint8_t PopByte();

  uint8_t PopUint8();
  int8_t PopInt8();

  uint16_t PopUint16();
  int16_t PopInt16();

  uint32_t PopUint32();
  int32_t PopInt32();

  uint64_t PopUint64();
  int64_t PopInt64();

  void Memcpy(void *dst, size_t n);

  void Skip(size_t n);

 private:
  void RequireAtLeast(size_t n);

  byte *__restrict__ const ptr;
  byte *__restrict__ begin;
  byte *__restrict__ end;

  DefaultAllocator allocator;

  InputDescriptor stream;
};

}  // namespace pdp

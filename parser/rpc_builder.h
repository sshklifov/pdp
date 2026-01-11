#pragma once

#include "strings/string_builder.h"

namespace pdp {

template <typename Alloc = DefaultAllocator>
struct ByteBuilder : public SmallBufferStorage<byte, Alloc> {
  using SmallBufferStorage<byte, Alloc>::begin;
  using SmallBufferStorage<byte, Alloc>::end;
  using SmallBufferStorage<byte, Alloc>::limit;

  void Append(const void *data, size_t bytes) {
    this->ReserveFor(bytes);
    memcpy(end, data, bytes);
    end += bytes;
  }

  void SetByte(size_t pos, byte b) {
    pdp_assert(pos < this->Size());
    memcpy(begin + pos, &b, sizeof(b));
  }

  void AppendByteUnchecked(byte b) {
    pdp_assert(end < limit);
    memcpy(end, &b, sizeof(b));
    ++end;
  }

  void AppendByte(byte b) {
    this->ReserveFor(1);
    AppendByteUnchecked(b);
  }

  const void *Data() const { return begin; }
};

struct RpcBuilder;

struct RpcBuilderArrayRAII {
  RpcBuilderArrayRAII(RpcBuilder &b);
  ~RpcBuilderArrayRAII();

 private:
  RpcBuilder &builder;
};

struct RpcBytes {
  const void *data;
  size_t bytes;
};

struct RpcBuilder {
  friend struct RpcBuilderArrayRAII;

  RpcBuilder(uint32_t token, const StringSlice &method);

  void AddInteger(int32_t value);
  void AddUnsigned(uint32_t value);
  void AddBoolean(bool value);
  void AddString(const StringSlice &str);

  [[nodiscard]] RpcBuilderArrayRAII AddArray();

  [[nodiscard]] RpcBytes Finish();

  static constexpr uint32_t kMaxDepth = 8;

 private:
  void OpenShortArray();
  void CloseShortArray();

  void BackfillArrayElem();

  void PushByte(byte b);

  void PushUint8(uint8_t x);
  void PushInt8(int8_t x);

  void PushUint16(uint16_t x);
  void PushInt16(int16_t x);

  void PushUint32(uint32_t x);
  void PushInt32(int32_t x);

  struct Backfill {
    uint32_t pos;
    uint32_t num_elems;
  };

  Backfill array_backfill[kMaxDepth];
  int32_t depth;
  ByteBuilder<DefaultAllocator> builder;
};

};  // namespace pdp

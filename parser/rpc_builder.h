#pragma once

#include <initializer_list>
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

struct RpcBytes {
  const void *data;
  size_t bytes;
};

struct RpcBuilder {
  friend struct RpcBuilderArrayRAII;

  RpcBuilder(uint32_t token, const StringSlice &method);

  void Add(uint32_t value);
  void Add(int32_t value);
  void Add(int64_t value);
  void Add(uint64_t value);
  void Add(bool value);
  void Add(const StringSlice &str);
  void Add(const char *str) = delete;

  void Add(std::initializer_list<StringSlice> ilist) {
    OpenShortArray();
    for (const auto &item : ilist) {
      Add(item);
    }
    CloseShortArray();
  }

  [[nodiscard]] RpcBytes Finish();

  static constexpr uint32_t kMaxDepth = 8;

  void OpenShortArray();
  void CloseShortArray();

 private:
  void BackfillArrayElem();

  void PushByte(byte b);

  void PushUint8(uint8_t x);
  void PushInt8(int8_t x);

  void PushUint16(uint16_t x);
  void PushInt16(int16_t x);

  void PushUint32(uint32_t x);
  void PushInt32(int32_t x);

  void PushUint64(uint64_t x);
  void PushInt64(int64_t x);

  struct Backfill {
    uint32_t pos;
    uint32_t num_elems;
  };

  Backfill array_backfill[kMaxDepth];
  int32_t depth;
  ByteBuilder<DefaultAllocator> builder;
};

template <typename T>
struct IsRpc : std::false_type {};

template <>
struct IsRpc<int32_t> : std::true_type {};

template <>
struct IsRpc<uint32_t> : std::true_type {};

template <>
struct IsRpc<int64_t> : std::true_type {};

template <>
struct IsRpc<uint64_t> : std::true_type {};

template <>
struct IsRpc<bool> : std::true_type {};

template <>
struct IsRpc<StringSlice> : std::true_type {};

template <>
struct IsRpc<std::initializer_list<StringSlice>> : std::true_type {};

template <typename T>
inline constexpr bool IsRpcV = IsRpc<T>::value;

};  // namespace pdp

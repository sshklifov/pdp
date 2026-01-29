#include "rpc_builder.h"
#include "core/internals.h"

namespace pdp {

RpcBuilder::RpcBuilder(uint32_t token, const StringSlice &method) { Restart(token, method); }

RpcBuilder::RpcBuilder(const StringSlice &method) {
  uint32_t placeholder_token = std::numeric_limits<uint32_t>::max();
  Restart(placeholder_token, method);
}

void RpcBuilder::Restart(uint32_t token, const StringSlice &method) {
  backfill[0].pos = 0;
  backfill[0].num_elems = 1;
  depth = 0;

  builder.Clear();
  PushByte(0x90);
  PushByte(0x0);

  Add(token);
  Add(method);
}

void RpcBuilder::PushByte(byte b) { builder.AppendByte(b); }

void RpcBuilder::PushUint8(uint8_t x) {
  builder.ReserveFor(1);
  builder.AppendByteUnchecked(static_cast<byte>(x));
}

void RpcBuilder::PushInt8(int8_t x) { PushUint8(BitCast<uint8_t>(x)); }

void RpcBuilder::PushUint16(uint16_t x) {
  builder.ReserveFor(2);
  builder.AppendByteUnchecked(static_cast<byte>((x >> 8) & 0xFF));
  builder.AppendByteUnchecked(static_cast<byte>(x & 0xFF));
}

void RpcBuilder::PushInt16(int16_t x) { PushUint16(BitCast<uint16_t>(x)); }

void RpcBuilder::PushUint32(uint32_t x) {
  builder.ReserveFor(4);
  builder.AppendByteUnchecked(static_cast<byte>((x >> 24) & 0xFF));
  builder.AppendByteUnchecked(static_cast<byte>((x >> 16) & 0xFF));
  builder.AppendByteUnchecked(static_cast<byte>((x >> 8) & 0xFF));
  builder.AppendByteUnchecked(static_cast<byte>(x & 0xFF));
}

void RpcBuilder::PushInt32(int32_t x) { PushUint32(BitCast<uint32_t>(x)); }

void RpcBuilder::PushUint64(uint64_t x) {
  builder.ReserveFor(8);
  builder.AppendByteUnchecked(static_cast<byte>((x >> 56) & 0xFF));
  builder.AppendByteUnchecked(static_cast<byte>((x >> 48) & 0xFF));
  builder.AppendByteUnchecked(static_cast<byte>((x >> 40) & 0xFF));
  builder.AppendByteUnchecked(static_cast<byte>((x >> 32) & 0xFF));
  builder.AppendByteUnchecked(static_cast<byte>((x >> 24) & 0xFF));
  builder.AppendByteUnchecked(static_cast<byte>((x >> 16) & 0xFF));
  builder.AppendByteUnchecked(static_cast<byte>((x >> 8) & 0xFF));
  builder.AppendByteUnchecked(static_cast<byte>(x & 0xFF));
}

void RpcBuilder::PushInt64(int64_t x) { PushUint64(BitCast<uint64_t>(x)); }

void RpcBuilder::Add(uint32_t x) {
  if (x <= 0x7f) {
    PushUint8(x);
  } else if (x <= std::numeric_limits<uint8_t>::max()) {
    PushByte(0xcc);
    PushUint8(x);
  } else if (x <= std::numeric_limits<uint16_t>::max()) {
    PushByte(0xcd);
    PushUint16(x);
  } else {
    PushByte(0xce);
    PushUint32(x);
  }

  OnElementAdded();
}

void RpcBuilder::Add(uint64_t x) {
  if (PDP_UNLIKELY(x > std::numeric_limits<uint32_t>::max())) {
    PushByte(0xcf);
    PushUint64(x);
    OnElementAdded();
  } else {
    Add(static_cast<uint32_t>(x));
  }
}

void RpcBuilder::Add(int64_t x) {
  if (PDP_UNLIKELY(x < std::numeric_limits<int32_t>::min())) {
    PushByte(0xd3);
    PushInt64(x);
    OnElementAdded();
  } else {
    Add(static_cast<int32_t>(x));
  }
}

void RpcBuilder::Add(int32_t x) {
  if (x >= 0) {
    return Add(BitCast<uint32_t>(x));
  }

  if (x >= -32) {
    PushInt8(x);
  } else if (x >= std::numeric_limits<int8_t>::min()) {
    PushByte(0xd0);
    PushInt8(x);
  } else if (x >= std::numeric_limits<int16_t>::min()) {
    PushByte(0xd1);
    PushInt16(x);
  } else {
    PushByte(0xd2);
    PushInt32(x);
  }

  OnElementAdded();
}

void RpcBuilder::Add(bool value) {
  PushByte(value ? 0xc3 : 0xc2);

  OnElementAdded();
}

char *RpcBuilder::AddUninitializedString(size_t length) {
  if (length < 32) {
    byte b = 0xa0 | length;
    PushByte(b);
  } else if (length <= std::numeric_limits<uint8_t>::max()) {
    PushByte(0xd9);
    PushUint8(length);
  } else if (length <= std::numeric_limits<uint16_t>::max()) {
    PushByte(0xda);
    PushUint16(length);
  } else if (length <= std::numeric_limits<uint32_t>::max()) {
    PushByte(0xdb);
    PushUint32(length);
  } else {
    PDP_UNREACHABLE("RpcBuilder: string overflow!");
  }
  OnElementAdded();

  return reinterpret_cast<char *>(builder.AppendUninitialized(length));
}

void RpcBuilder::Add(const StringSlice &str) {
  char *out = AddUninitializedString(str.Length());
  memcpy(out, str.Data(), str.Size());
}

void RpcBuilder::Add(const char *str) { Add(StringSlice(str)); }

void RpcBuilder::OnElementAdded() {
  pdp_assert(depth >= 0);
  backfill[depth].num_elems += 1;
}

void RpcBuilder::OpenShortArray() {
  OnElementAdded();
  if (PDP_UNLIKELY(depth + 1 == kMaxDepth)) {
    PDP_UNREACHABLE("RpcBuilder: depth overflow!");
  }
  ++depth;
  backfill[depth].pos = builder.Size();
  backfill[depth].num_elems = 0;

  PushByte(0x90);
}

void RpcBuilder::CloseShortArray() {
  if (PDP_UNLIKELY(depth <= 0)) {
    PDP_UNREACHABLE("RpcBuilder: Closing list which has not been declared!");
  }
  auto pos = backfill[depth].pos;
  auto num_elems = backfill[depth].num_elems;
  if (PDP_UNLIKELY(num_elems > 15)) {
    PDP_UNREACHABLE("RpcBuilder: Too many elements for list!");
  }
  byte b = 0x90 | num_elems;
  builder.SetByte(pos, b);

  --depth;
}

void RpcBuilder::OpenShortMap() {
  OnElementAdded();
  if (PDP_UNLIKELY(depth + 1 == kMaxDepth)) {
    PDP_UNREACHABLE("RpcBuilder: depth overflow!");
  }
  ++depth;
  backfill[depth].pos = builder.Size();
  backfill[depth].num_elems = 0;

  PushByte(0x80);
}

void RpcBuilder::CloseShortMap() {
  if (PDP_UNLIKELY(depth <= 0)) {
    PDP_UNREACHABLE("RpcBuilder: Closing map which has not been declared!");
  }
  auto pos = backfill[depth].pos;
  if (PDP_UNLIKELY(backfill[depth].num_elems % 2 == 1)) {
    PDP_UNREACHABLE("RpcBuilder: Odd number of arguments for map!");
  }
  auto num_elems = backfill[depth].num_elems / 2;
  if (PDP_UNLIKELY(num_elems > 15)) {
    PDP_UNREACHABLE("RpcBuilder: Too many elements for map!");
  }
  byte b = 0x80 | num_elems;
  builder.SetByte(pos, b);

  --depth;
}

bool RpcBuilder::SetRequestToken(uint32_t t) {
  const size_t token_pos = 2;
  const bool can_replace = builder[token_pos] == 0xce;
  pdp_assert(can_replace);
  if (PDP_UNLIKELY(!can_replace)) {
    return false;
  }
  builder[token_pos + 1] = static_cast<byte>((t >> 24) & 0xFF);
  builder[token_pos + 2] = static_cast<byte>((t >> 16) & 0xFF);
  builder[token_pos + 3] = static_cast<byte>((t >> 8) & 0xFF);
  builder[token_pos + 4] = static_cast<byte>(t & 0xFF);
  return true;
}

RpcBytes RpcBuilder::Finish() {
  if (PDP_UNLIKELY(depth != 0)) {
    PDP_UNREACHABLE("RpcBuilder: Unclosed array!");
  }

  pdp_assert(backfill[0].num_elems == 4);
  byte start_byte = 0x90 | backfill[0].num_elems;
  builder.SetByte(0, start_byte);

#ifdef PDP_ENABLE_ASSERT
  depth = -1;  // Will trigger asserts if object is reused
#endif
  return {builder.Data(), builder.Size()};
}

};  // namespace pdp

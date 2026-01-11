#include "rpc_builder.h"
#include "core/internals.h"

namespace pdp {

RpcBuilderArrayRAII::RpcBuilderArrayRAII(RpcBuilder &b) : builder(b) { builder.OpenShortArray(); }

RpcBuilderArrayRAII::~RpcBuilderArrayRAII() { builder.CloseShortArray(); }

RpcBuilder::RpcBuilder(uint32_t token, const StringSlice &method) {
  array_backfill[0].pos = 0;
  array_backfill[0].num_elems = 1;
  depth = 0;
  PushByte(0x90);
  PushByte(0x0);

  AddUnsigned(token);
  AddString(method);
}

void RpcBuilder::PushByte(byte b) { builder.AppendByte(b); }

void RpcBuilder::PushUint8(uint8_t x) {
  builder.ReserveFor(1);
  builder.AppendByteUnchecked(static_cast<byte>(x));
}

void RpcBuilder::PushInt8(int8_t x) { PushUint8(BitCast(x)); }

void RpcBuilder::PushUint16(uint16_t x) {
  builder.ReserveFor(2);
  builder.AppendByteUnchecked(static_cast<byte>((x >> 8) & 0xFF));
  builder.AppendByteUnchecked(static_cast<byte>(x & 0xFF));
}

void RpcBuilder::PushInt16(int16_t x) { PushUint16(BitCast(x)); }

void RpcBuilder::PushUint32(uint32_t x) {
  builder.ReserveFor(4);
  builder.AppendByteUnchecked(static_cast<byte>((x >> 24) & 0xFF));
  builder.AppendByteUnchecked(static_cast<byte>((x >> 16) & 0xFF));
  builder.AppendByteUnchecked(static_cast<byte>((x >> 8) & 0xFF));
  builder.AppendByteUnchecked(static_cast<byte>(x & 0xFF));
}

void RpcBuilder::PushInt32(int32_t x) { PushUint32(BitCast(x)); }

void RpcBuilder::AddUnsigned(uint32_t x) {
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

  BackfillArrayElem();
}

void RpcBuilder::AddInteger(int32_t x) {
  if (x >= 0) {
    return AddUnsigned(BitCast(x));
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

  BackfillArrayElem();
}

void RpcBuilder::AddBoolean(bool value) {
  PushByte(value ? 0xc3 : 0xc2);

  BackfillArrayElem();
}

void RpcBuilder::AddString(const StringSlice &str) {
  if (str.Length() < 32) {
    byte length = 0xa0 | str.Length();
    PushByte(length);
  } else if (str.Length() <= std::numeric_limits<uint8_t>::max()) {
    PushByte(0xd9);
    PushUint8(str.Length());
  } else if (str.Length() <= std::numeric_limits<uint16_t>::max()) {
    PushByte(0xda);
    PushUint16(str.Length());
  } else if (str.Length() <= std::numeric_limits<uint32_t>::max()) {
    PushByte(0xdb);
    PushUint32(str.Length());
  } else {
    PDP_UNREACHABLE("RpcBuilder: string overflow!");
  }
  builder.Append(str.Data(), str.Size());

  BackfillArrayElem();
}

void RpcBuilder::BackfillArrayElem() {
  pdp_assert(depth >= 0);
  array_backfill[depth].num_elems += 1;
}

void RpcBuilder::OpenShortArray() {
  BackfillArrayElem();
  if (PDP_UNLIKELY(depth + 1 == kMaxDepth)) {
    PDP_UNREACHABLE("RpcBuilder: array overflow!");
  }
  ++depth;
  array_backfill[depth].pos = builder.Size();
  array_backfill[depth].num_elems = 0;

  PushByte(0x90);
}

void RpcBuilder::CloseShortArray() {
  if (PDP_UNLIKELY(depth <= 0)) {
    PDP_UNREACHABLE("RpcBuilder: Closing list which has not been declared!");
  }
  auto pos = array_backfill[depth].pos;
  auto num_elems = array_backfill[depth].num_elems;
  if (PDP_UNLIKELY(num_elems > 15)) {
    PDP_UNREACHABLE("RpcBuilder: Too many elements for list!");
  }
  byte b = 0x90 | num_elems;
  builder.SetByte(pos, b);

  --depth;
}

RpcBuilderArrayRAII RpcBuilder::AddArray() { return RpcBuilderArrayRAII(*this); }

RpcBytes RpcBuilder::Finish() {
  if (PDP_UNLIKELY(depth != 0)) {
    PDP_UNREACHABLE("RpcBuilder: Unclosed array!");
  }

  pdp_assert(array_backfill[0].num_elems == 4);
  byte start_byte = 0x90 | array_backfill[0].num_elems;
  builder.SetByte(0, start_byte);

#ifdef PDP_ENABLE_ASSERT
  depth = -1;  // Will trigger asserts if object is reused
#endif
  return {builder.Data(), builder.Size()};
}

};  // namespace pdp

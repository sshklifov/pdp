#include "rpc_parser.h"
#include "core/log.h"
#include "external/ankerl_hash.h"

namespace pdp {

uint32_t ReadRpcArrayLength(ByteStream &s) {
  byte b = s.PopByte();
  switch (b) {
      // Array with 2 byte length
    case 0xdc:
      return s.PopUint16();
      // Array with 4 byte length
    case 0xdd:
      return s.PopUint32();
  }
  if (PDP_LIKELY(b >= 0x90 && b <= 0x9f)) {
    byte length_from_byte = (b & 0xf);
    return length_from_byte;
  }

  pdp_critical("RPC byte: {}", MakeHex(b));
  PDP_UNREACHABLE("Unexpected RPC byte, expecting array");
}

void ExpectRpcArrayWithLength(ByteStream &s, uint32_t length) {
  uint32_t read_length = ReadRpcArrayLength(s);
  if (PDP_UNLIKELY(read_length != length)) {
    pdp_critical("Got length of {}", read_length);
    PDP_UNREACHABLE("Unexpected RPC array length");
  }
}

void ExpectRpcInteger(ByteStream &s, int64_t what) {
  int64_t integer = ReadRpcInteger(s);
  if (PDP_UNLIKELY(integer != what)) {
    pdp_critical("Got integer {} but got {}", integer, what);
    PDP_UNREACHABLE("Read the wrong RPC integer");
  }
}

int64_t ReadRpcInteger(ByteStream &s) {
  byte b = s.PopByte();
  switch (b) {
      // 8bit signed
    case 0xd0:
      return s.PopInt8();
      // 16bit signed
    case 0xd1:
      return s.PopInt16();
      // 32bit signed
    case 0xd2:
      return s.PopInt32();
      // 64bit signed
    case 0xd3:
      return s.PopInt64();
      // 8bit unsigned
    case 0xcc:
      return s.PopUint8();
      // 16bit unsigned
    case 0xcd:
      return s.PopUint16();
      // 32bit unsigned
    case 0xce:
      return s.PopUint32();
      // 64bit unsigned
    case 0xcf:
      return s.PopUint64();

      // 8 bit signed EXT (the type is not relevant)
    case 0xd4:
      s.PopInt8();
      return s.PopInt8();
      // 16 bit signed EXT (the type is not relevant)
    case 0xd5:
      s.PopInt8();
      return s.PopInt16();
      // 32 bit signed EXT (the type is not relevant)
    case 0xd6:
      s.PopInt8();
      return s.PopInt32();
      // 64 bit signed EXT (the type is not relevant)
    case 0xd7:
      s.PopInt8();
      return s.PopInt64();
  }
  static_assert(std::is_unsigned_v<byte>, "Possible underflow in cast detected");
  if (PDP_LIKELY(b <= 0x7f)) {
    return b;
  } else if (PDP_LIKELY(b >= 0xe0)) {
    return BitCast<int8_t>(b);
  } else {
    pdp_critical("RPC byte: {}", MakeHex(b));
    PDP_UNREACHABLE("Unexpected RPC byte, expecting an integer");
  }
}

static DynamicString BuildDynamicString(uint32_t length, ByteStream &s) {
  DynamicString res;
  impl::_InPlaceStringInit string_init(res);
  char *buf = string_init(length);
  s.Memcpy(buf, length);
  return res;
}

DynamicString ReadRpcString(ByteStream &s) {
  byte b = s.PopByte();
  switch (b) {
      // String with 1 byte length
    case 0xd9:
      return BuildDynamicString(s.PopUint8(), s);

      // String with 2 byte length
    case 0xda:
      return BuildDynamicString(s.PopUint16(), s);
      // String with 4 byte length
    case 0xdb:
      return BuildDynamicString(s.PopUint32(), s);
  }
  if (b >= 0xa0 && b <= 0xbf) {
    return BuildDynamicString(b & 0x1f, s);
  }

  pdp_critical("RPC byte: {}", MakeHex(b));
  PDP_UNREACHABLE("Unexpected RPC byte, expecting string");
}

bool ReadRpcBoolean(ByteStream &s) {
  byte b = s.PopByte();
  if (PDP_LIKELY((b | 1) == 0xc3)) {
    return b & 0x1;
  }
  pdp_critical("RPC byte: {}", MakeHex(b));
  PDP_UNREACHABLE("Unexpected RPC byte, expecting a boolean");
}

void SkipRpcError(ByteStream &s) {
  auto byte = s.PeekByte();
  if (PDP_LIKELY(byte == 0xc0)) {
    s.PopByte();
    return;
  }
  RpcChunkArrayPass pass(s);
  StrongTypedView expr = pass.Parse();
  StringBuilder builder;
  expr.ToJson(builder);
  pdp_critical("Error: {}", builder.GetSlice());
  PDP_UNREACHABLE("Expecting null as error response TODO");
  // TODO
}

namespace impl {

template <typename A>
_RpcPassHelper<A>::_RpcPassHelper(ByteStream &input) : stream(input) {}

template <typename A>
ExprBase *_RpcPassHelper<A>::Parse() {
  ExprBase *root = BigAssSwitch();
  PushNesting(root);
  if (PDP_UNLIKELY(nesting_stack.Empty())) {
    PDP_UNREACHABLE("Top level RPC record is not an array or map!");
  }

  while (!nesting_stack.Empty()) {
    ExprBase *expr = BigAssSwitch();
    AttachExpr(expr);
    PushNesting(expr);
  }
  return root;
}

template <typename A>
void _RpcPassHelper<A>::AttachExpr(ExprBase *expr) {
  auto &top = nesting_stack.Top();

  *top.elements = expr;
  ++top.elements;
  const bool is_key = (top.remaining % 2 == 0);

  if (is_key && top.hashes) {
    if (PDP_LIKELY(expr->kind == ExprBase::kString)) {
      const char *str = (char *)expr + sizeof(ExprBase);
      *top.hashes = ankerl::unordered_dense::hash(str, expr->size);
      ++top.hashes;
    } else if (PDP_LIKELY(expr->kind == ExprBase::kInt)) {
      ExprInt *integer = static_cast<ExprInt *>(expr);
      *top.hashes = ankerl::unordered_dense::hash(integer->value);
      ++top.hashes;
    } else {
      pdp_critical("RPC map has unsupported key type: {}!",
                   StringSlice(ExprKindToString(expr->kind)));
      PDP_UNREACHABLE("Cannot parse rpc");
    }
  }

  top.remaining -= 1;
  if (PDP_UNLIKELY(top.remaining == 0)) {
    nesting_stack.Pop();
  }
}

template <typename A>
void _RpcPassHelper<A>::PushNesting(ExprBase *expr) {
  if (PDP_LIKELY(expr->size > 0)) {
    if (expr->kind == ExprBase::kList) {
      auto *record = nesting_stack.NewElement();
      record->elements = reinterpret_cast<ExprBase **>((uint8_t *)expr + sizeof(ExprList));
      record->remaining = expr->size;
      record->hashes = nullptr;
    } else if (expr->kind == ExprBase::kMap) {
      auto *record = nesting_stack.NewElement();
      ExprMap *map = static_cast<ExprMap *>(expr);
      record->elements = (ExprBase **)map->pairs;
      record->remaining = 2 * map->size;
      record->hashes = map->hashes;
    }
  }
}

template <typename A>
ExprBase *_RpcPassHelper<A>::CreateNull() {
  ExprBase *expr = static_cast<ExprBase *>(allocator.AllocateUnchecked(sizeof(ExprBase)));
  expr->kind = ExprBase::kNull;
  return expr;
}

template <typename A>
ExprBase *_RpcPassHelper<A>::CreateInteger(int64_t value) {
  ExprInt *expr =
      static_cast<ExprInt *>(allocator.AllocateUnchecked(sizeof(ExprBase) + sizeof(int64_t)));
  expr->kind = ExprBase::kInt;
  expr->value = value;
  return expr;
}

template <typename A>
ExprBase *_RpcPassHelper<A>::CreateString(uint32_t length) {
  ExprString *expr = static_cast<ExprString *>(allocator.Allocate(sizeof(ExprBase) + length));
  expr->kind = ExprBase::kString;
  expr->size = length;
  stream.Memcpy(expr->payload, length);
  return expr;
}

template <typename A>
ExprBase *_RpcPassHelper<A>::CreateArray(uint32_t length) {
  ExprList *expr = static_cast<ExprList *>(
      allocator.AllocateUnchecked(sizeof(ExprList) + sizeof(ExprBase *) * length));
  expr->kind = ExprBase::kList;
  expr->size = length;

  return expr;
}

template <typename A>
ExprBase *_RpcPassHelper<A>::CreateMap(uint32_t length) {
  ExprMap *expr = static_cast<ExprMap *>(allocator.AllocateUnchecked(sizeof(ExprMap)));
  expr->kind = ExprBase::kMap;
  expr->hashes = static_cast<uint32_t *>(allocator.AllocateOrNull(length * sizeof(uint32_t)));
  expr->pairs =
      static_cast<ExprMap::Pair *>(allocator.AllocateOrNull(length * sizeof(ExprMap::Pair)));
  expr->size = length;
  return expr;
}

template <typename A>
ExprBase *_RpcPassHelper<A>::BigAssSwitch() {
  byte b = stream.PopByte();
  switch (b) {
      // 8bit signed
    case 0xd0:
      return CreateInteger(stream.PopInt8());
      // 16bit signed
    case 0xd1:
      return CreateInteger(stream.PopInt16());
      // 32bit signed
    case 0xd2:
      return CreateInteger(stream.PopInt32());
      // 64bit signed
    case 0xd3:
      return CreateInteger(stream.PopInt64());
      // 8bit unsigned
    case 0xcc:
      return CreateInteger(stream.PopUint8());
      // 16bit unsigned
    case 0xcd:
      return CreateInteger(stream.PopUint16());
      // 32bit unsigned
    case 0xce:
      return CreateInteger(stream.PopUint32());
      // 64bit unsigned
    case 0xcf:
      return CreateInteger(stream.PopUint64());

      // 8 bit signed EXT (the type is not relevant)
    case 0xd4:
      stream.PopInt8();
      return CreateInteger(stream.PopInt8());
      // 16 bit signed EXT (the type is not relevant)
    case 0xd5:
      stream.PopInt8();
      return CreateInteger(stream.PopInt16());
      // 32 bit signed EXT (the type is not relevant)
    case 0xd6:
      stream.PopInt8();
      return CreateInteger(stream.PopInt32());
      // 64 bit signed EXT (the type is not relevant)
    case 0xd7:
      stream.PopInt8();
      return CreateInteger(stream.PopInt64());

      // null
    case 0xc0:
      return CreateNull();

    case 0xc2:
    case 0xc3:
      return CreateInteger(b & 0x1);

      // String with 1 byte length
    case 0xd9:
      return CreateString(stream.PopUint8());
      // String with 2 byte length
    case 0xda:
      return CreateString(stream.PopUint16());
      // String with 4 byte length
    case 0xdb:
      return CreateString(stream.PopUint32());

      // Array with 2 byte length
    case 0xdc:
      return CreateArray(stream.PopUint16());
      // Array with 4 byte length
    case 0xdd:
      return CreateArray(stream.PopUint32());

      // Map with 2 byte length
    case 0xde:
      return CreateMap(stream.PopUint16());
      // Array with 4 byte length
    case 0xdf:
      return CreateMap(stream.PopUint32());
  }

  if (b <= 0x7f) {
    return CreateInteger((uint8_t)b);
  } else if (b >= 0xe0) {
    return CreateInteger((int8_t)b);
  } else if (b >= 0xa0 && b <= 0xbf) {
    return CreateString(b & 0x1f);
  } else if (b >= 0x90 && b <= 0x9f) {
    return CreateArray(b & 0xf);
  } else if (b >= 0x80 && b <= 0x8f) {
    return CreateMap(b & 0xf);
  }
  // pdp_critical("Unsupported RPC byte: {}", b);
  PDP_UNREACHABLE("Cannot parse rpc");
}

template <typename A>
A &_RpcPassHelper<A>::GetAllocator() {
  return allocator;
}

}  // namespace impl

template struct impl::_RpcPassHelper<ChunkArray>;

}  // namespace pdp

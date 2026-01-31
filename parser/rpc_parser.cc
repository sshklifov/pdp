#include "rpc_parser.h"
#include "core/log.h"
#include "external/ankerl_hash.h"

namespace pdp {

RpcKind ClassifyRpcByte(byte b) {
  if (b <= 0x7f || b >= 0xe0) {
    return RpcKind::kInteger;
  } else if (b >= 0xa0 && b <= 0xbf) {
    return RpcKind::kString;
  } else if (b >= 0x90 && b <= 0x9f) {
    return RpcKind::kArray;
  } else if (b >= 0x80 && b <= 0x8f) {
    return RpcKind::kMap;
  }

  switch (b) {
    case 0xc2:
    case 0xc3:
      return RpcKind::kBool;

    case 0xd0:
    case 0xd1:
    case 0xd2:
    case 0xd3:
    case 0xcc:
    case 0xcd:
    case 0xce:
    case 0xcf:
    case 0xd4:
    case 0xd5:
    case 0xd6:
    case 0xd7:
      return RpcKind::kInteger;

      // null
    case 0xc0:
      return RpcKind::kNull;

    case 0xd9:
    case 0xda:
    case 0xdb:
      return RpcKind::kString;

    case 0xdc:
    case 0xdd:
      return RpcKind::kArray;

    case 0xde:
    case 0xdf:
      return RpcKind::kMap;
  }

  PDP_FMT_UNREACHABLE("Cannot parse rpc, unexpected byte: {}", MakeHex(b));
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
    PDP_FMT_UNREACHABLE("Unexpected RPC byte {}, expecting an integer", MakeHex(b));
  }
}

bool ReadRpcBoolean(ByteStream &s) {
  byte b = s.PopByte();
  if (PDP_LIKELY((b | 1) == 0xc3)) {
    return b & 0x1;
  }
  PDP_FMT_UNREACHABLE("Unexpected RPC byte {}, expecting a boolean", MakeHex(b));
}

FixedString ReadRpcString(ByteStream &s) {
  auto length = ReadRpcStringLength(s);
  StringBuffer buffer(length + 1);
  s.Memcpy(buffer.Get(), length);
  buffer[length] = 0;
  return FixedString(std::move(buffer), length);
}

uint32_t ReadRpcStringLength(ByteStream &s) {
  byte b = s.PopByte();
  switch (b) {
      // String with 1 byte length
    case 0xd9:
      return s.PopUint8();

      // String with 2 byte length
    case 0xda:
      return s.PopUint16();
      // String with 4 byte length
    case 0xdb:
      return s.PopUint32();
  }
  if (b >= 0xa0 && b <= 0xbf) {
    return b & 0x1f;
  }

  PDP_FMT_UNREACHABLE("Unexpected RPC byte {}, expecting string", MakeHex(b));
}

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

  PDP_FMT_UNREACHABLE("Unexpected RPC byte {}, expecting array", MakeHex(b));
}

uint32_t ReadRpcMapLength(ByteStream &s) {
  byte b = s.PopByte();
  switch (b) {
      // Map with 2 byte length
    case 0xde:
      return s.PopUint16();
      // Map with 4 byte length
    case 0xdf:
      return s.PopUint32();
  }
  if (PDP_LIKELY(b >= 0x80 && b <= 0x8f)) {
    byte length_from_byte = (b & 0xf);
    return length_from_byte;
  }

  PDP_FMT_UNREACHABLE("Unexpected RPC byte {}, expecting map", MakeHex(b));
}

void SkipRpcValue(ByteStream &s) {
  uint32_t skip_items = 1;
  do {
    --skip_items;
    byte b = s.PeekByte();
    switch (ClassifyRpcByte(b)) {
      case RpcKind::kNull:
        s.PopByte();
        break;
      case RpcKind::kBool:
        s.PopByte();
        break;
      case RpcKind::kInteger:
        (void)ReadRpcInteger(s);
        break;
      case RpcKind::kString:
        s.Skip(ReadRpcStringLength(s));
        break;
      case RpcKind::kArray:
        skip_items += ReadRpcArrayLength(s);
        break;
      case RpcKind::kMap:
        skip_items += 2 * ReadRpcMapLength(s);
        break;
      default:
        pdp_assert(false);
    }
  } while (skip_items);
}

bool FormatRpcError(ByteStream &s, StringBuilder<DefaultAllocator> &out) {
  // Fast path: there is no error.

  byte b = s.PeekByte();
  if (PDP_LIKELY(b == 0xc0)) {
    s.PopByte();
    return false;
  }

  // Unknown: Do we need a stack? Or is a scalar value being processed only.
  // This is important for the main invariant, not for speed.

  struct Frame {
    Frame(uint32_t length, uint32_t is_map) : length(length), is_map(is_map) {}
    uint32_t length;
    uint32_t is_map;
  };
  Stack<Frame> stack;

  uint32_t length = 0;
  switch (ClassifyRpcByte(b)) {
    case RpcKind::kBool:
      out.Append(ReadRpcBoolean(s));
      return true;
    case RpcKind::kInteger:
      out.Append(ReadRpcInteger(s));
      return true;
    case RpcKind::kString:
      length = ReadRpcStringLength(s);
      s.Memcpy(out.AppendUninitialized(length), length);
      return true;
    case RpcKind::kArray:
      length = ReadRpcArrayLength(s);
      if (PDP_UNLIKELY(length == 0)) {
        out.Append("[]");
        return true;
      }
      stack.ReserveFor(8);
      stack.Emplace(length, false);
      out.Append('[');
      break;
    case RpcKind::kMap:
      length = ReadRpcMapLength(s);
      if (PDP_UNLIKELY(length == 0)) {
        out.Append("{}");
        return true;
      }
      stack.ReserveFor(8);
      stack.Emplace(2 * length, true);
      out.Append('{');
      break;
    case pdp::RpcKind::kNull:
    default:
      pdp_assert(false);
  }

  // Slow path: Keep reading bytes and unwinding the stack until it's empty.

  for (;;) {
    switch (ClassifyRpcByte(s.PeekByte())) {
      case RpcKind::kNull:
        out.Append("null");
        break;
      case RpcKind::kBool:
        out.Append(ReadRpcBoolean(s));
        break;
      case RpcKind::kInteger:
        out.Append(ReadRpcInteger(s));
        break;
      case RpcKind::kString:
        length = ReadRpcStringLength(s);
        s.Memcpy(out.AppendUninitialized(length), length);
        break;
      case RpcKind::kArray:
        length = ReadRpcArrayLength(s);
        if (PDP_LIKELY(length > 0)) {
          stack.Emplace(length, false);
          out.Append('[');
          continue;
        } else {
          out.Append("[]");
          break;
        }
      case RpcKind::kMap:
        length = ReadRpcMapLength(s);
        if (PDP_LIKELY(length > 0)) {
          stack.Emplace(length, true);
          out.Append('{');
          continue;
        } else {
          out.Append("{}");
          break;
        }
      default:
        pdp_assert(false);
    }

    stack.Top().length -= 1;

    if (PDP_LIKELY(stack.Top().length > 0)) {
      const bool is_equal = (stack.Top().is_map && stack.Top().length % 2 == 1);
      char pun = (0x2 | is_equal) << 4 | (0xc | is_equal);
      out.Append(pun);
    } else {
      do {
        char bracket = ']' | (stack.Top().is_map << 6);
        out.Append(bracket);
        stack.Pop();
      } while (!stack.Empty() && stack.Top().length <= 1);
      if (stack.Empty()) {
        return true;
      }
    }
  }
}

bool PrintRpcError(uint32_t token, ByteStream &s) {
  StringBuilder<DefaultAllocator> out;
  if (FormatRpcError(s, out)) {
    pdp_error("RPC error with token={}: {}", token, out.ToSlice());
    return true;
  } else {
    return false;
  }
}

// struct SkipPolicy {
//   void OnNull() {}
//   void OnBool(bool) {}
//   void OnInteger(int64_t) {}

//   void OnString(ByteStream &s, uint32_t length) { s.Skip(length); }

//   void OnArray(ByteStream &s, uint32_t length) {
//   }
// };

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
      PDP_FMT_UNREACHABLE("RPC map has unsupported key type: {}!",
                          StringSlice(ExprKindToString(expr->kind)));
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
  PDP_FMT_UNREACHABLE("Unsupported RPC byte: {}", MakeHex(b));
}

template <typename A>
A &_RpcPassHelper<A>::GetAllocator() {
  return allocator;
}

}  // namespace impl

template struct impl::_RpcPassHelper<ChunkArray>;

}  // namespace pdp

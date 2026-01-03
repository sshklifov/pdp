#include "rpc_parser.h"

namespace pdp {

ExprBase *RpcPass::Parse() {
  ExprBase *root = BigAssSwitch();
  PushNesting(root);
  if (PDP_UNLIKELY(nesting_stack.Empty())) {
    pdp_critical("Top level RPC record is not an array or map!");
    PDP_UNREACHABLE();
  }

  while (!nesting_stack.Empty()) {
    ExprBase *expr = BigAssSwitch();

    auto &top = nesting_stack.Top();
    *top.elements = expr;
    ++top.elements;
    top.remaining -= 1;
    if (PDP_UNLIKELY(top.remaining == 0)) {
      nesting_stack.Pop();
    }

    if (expr->kind == ExprBase::kList) {
      auto *record = nesting_stack.NewElement();
      record->elements = reinterpret_cast<ExprBase **>((uint8_t *)expr + sizeof(ExprBase));
      record->remaining = expr->size;
    } else if (expr->kind == ExprBase::kMap) {
      auto *record = nesting_stack.NewElement();
      record->elements = reinterpret_cast<ExprBase **>((uint8_t *)expr + sizeof(ExprBase));
      record->remaining = 2 * expr->size;
    }
  }
  return root;
}

void RpcPass::PushNesting(ExprBase *expr) {
  ExprBase **elements = reinterpret_cast<ExprBase **>((uint8_t *)expr + sizeof(ExprBase));
  auto *record = nesting_stack.NewElement();
  record->elements = elements;
  if (expr->kind == ExprBase::kList) {
    record->remaining = expr->size;
  } else if (expr->kind == ExprBase::kMap) {
    record->remaining = 2 * expr->size;
  }
}

ExprBase *RpcPass::CreateNull() {
  ExprBase *expr = static_cast<ExprBase *>(chunk_array.AllocateUnchecked(sizeof(ExprBase)));
  expr->kind = ExprBase::kNull;
  return expr;
}

ExprBase *RpcPass::CreateInteger(int64_t value) {
  ExprInt *expr =
      static_cast<ExprInt *>(chunk_array.AllocateUnchecked(sizeof(ExprBase) + sizeof(int64_t)));
  expr->kind = ExprBase::kInt;
  expr->value = value;
  return expr;
}

ExprBase *RpcPass::CreateString(uint32_t length) {
  ExprString *expr = static_cast<ExprString *>(chunk_array.Allocate(sizeof(ExprBase) + length));
  expr->kind = ExprBase::kString;
  expr->size = length;
  stream.Memcpy(expr->payload, length);
  return expr;
}

ExprBase *RpcPass::CreateArray(uint32_t length) {
  ExprList *expr = static_cast<ExprList *>(
      chunk_array.AllocateUnchecked(sizeof(ExprList) + sizeof(ExprBase *) * length));
  expr->kind = ExprBase::kList;
  expr->size = length;

  return expr;
}

ExprBase *RpcPass::CreateMap(uint32_t length) {
  ExprMap *expr = static_cast<ExprMap *>(
      chunk_array.AllocateUnchecked(sizeof(ExprMap) + sizeof(ExprBase *) * length * 2));
  expr->kind = ExprBase::kMap;
  expr->size = length;
  return expr;
}

ExprBase *RpcPass::BigAssSwitch() {
  unsigned char byte = stream.PopByte();
  switch (byte) {
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

      // null
    case 0xc0:
      return CreateNull();

    case 0xc2:
    case 0xc3:
      return CreateInteger(byte & 0x1);

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

  if (byte <= 0x7f) {
    return CreateInteger((uint8_t)byte);
  } else if (byte >= 0xe0) {
    return CreateInteger((int8_t)byte);
  } else if (byte >= 0xa0 && byte <= 0xbf) {
    return CreateString(byte & 0x1f);
  } else if (byte >= 0x90 && byte <= 0x9f) {
    return CreateArray(byte & 0xf);
  } else if (byte >= 0x80 && byte <= 0x8f) {
    return CreateMap(byte & 0xf);
  }
  pdp_critical("Unsupported RPC byte: {}", byte);
  PDP_UNREACHABLE();
}

}  // namespace pdp

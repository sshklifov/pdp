#pragma once

#include "chunk_array.h"
#include "expr.h"

#include "data/stack.h"
#include "strings/byte_stream.h"

#include <cstdint>

namespace pdp {

struct RpcPass {
  ExprBase *Parse();

 private:
  void PushNesting(ExprBase *expr);

  ExprBase *CreateNull();

  ExprBase *CreateInteger(int64_t value);

  ExprBase *CreateString(uint32_t length);

  ExprBase *CreateArray(uint32_t length);

  ExprBase *CreateMap(uint32_t length);

  ExprBase *BigAssSwitch();

  struct RpcRecord {
    ExprBase **elements;
    uint64_t remaining;
  };

  ByteStream stream;
  ChunkArray<DefaultAllocator> chunk_array;
  Stack<RpcRecord> nesting_stack;
};

}  // namespace pdp

#pragma once

#include "expr.h"

#include "data/chunk_array.h"
#include "data/stack.h"
#include "strings/byte_stream.h"
#include "strings/fixed_string.h"

namespace pdp {

enum class RpcKind { kNull, kInteger, kBool, kString, kArray, kMap };

RpcKind ClassifyRpcByte(byte b);
int64_t ReadRpcInteger(ByteStream &s);
bool ReadRpcBoolean(ByteStream &s);
FixedString ReadRpcString(ByteStream &s);

uint32_t ReadRpcStringLength(ByteStream &s);
uint32_t ReadRpcArrayLength(ByteStream &s);
uint32_t ReadRpcMapLength(ByteStream &s);

void SkipRpcValue(ByteStream &s);
bool FormatRpcError(ByteStream &s, StringBuilder<DefaultAllocator> &out);
bool PrintRpcError(uint32_t, ByteStream &s);

namespace impl {

template <typename Alloc>
struct _RpcPassHelper {
  _RpcPassHelper(ByteStream &input);

  ExprBase *Parse();

  Alloc &GetAllocator();

 private:
  void AttachExpr(ExprBase *expr);
  void PushNesting(ExprBase *expr);

  ExprBase *CreateNull();

  ExprBase *CreateInteger(int64_t value);

  ExprBase *CreateString(uint32_t length);

  ExprBase *CreateArray(uint32_t length);

  ExprBase *CreateMap(uint32_t length);

  ExprBase *BigAssSwitch();

  struct RpcRecord {
    ExprBase **elements;
    uint32_t *hashes;
    uint64_t remaining;
  };

  ByteStream &stream;
  Alloc allocator;
  Stack<RpcRecord> nesting_stack;
};

}  // namespace impl

struct RpcChunkArrayPass {
  RpcChunkArrayPass(ByteStream &input) : helper(input) {}

  ExprBase *Parse() { return helper.Parse(); }

  [[nodiscard]] ChunkHandle ReleaseChunks() { return helper.GetAllocator().ReleaseChunks(); }

 private:
  impl::_RpcPassHelper<ChunkArray> helper;
};

// TODO
// struct RpcArenaPass {
//   RpcArenaPass(int fd) : helper(fd),  {}
//   ExprBase *Parse() { return helper.Parse(); }
//   private:
//    impl::_RpcPassHelper<Arena<DefaultAllocator>> helper;
// };

}  // namespace pdp

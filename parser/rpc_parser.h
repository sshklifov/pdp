#pragma once

#include "expr.h"

#include "data/chunk_array.h"
#include "data/stack.h"
#include "strings/byte_stream.h"

namespace pdp {

namespace impl {

template <typename Alloc>
struct _RpcPassHelper {
  _RpcPassHelper(int fd);

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

  ByteStream stream;
  Alloc allocator;
  Stack<RpcRecord> nesting_stack;
};

}  // namespace impl

struct RpcChunkArrayPass {
  RpcChunkArrayPass(int fd) : helper(fd) {}

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

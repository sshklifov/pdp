#pragma once

#include "core/log.h"
#include "parser/rpc_builder.h"
#include "strings/byte_stream.h"
#include "strings/dynamic_string.h"
#include "strings/string_slice.h"

namespace pdp {

DynamicString Join(pdp::PackedValue *slots, uint64_t num_slots, uint64_t type_bits);

struct VimDriver {
  VimDriver(int input_fd, int output_fd);

  // Send RPC methods

  uint32_t NextToken() const;

  template <typename... Args>
  uint32_t SendRpcRequest(const StringSlice &method, Args &&...args) {
    static_assert((IsRpcV<std::decay_t<Args>> && ...));

#if PDP_TRACE_RPC_TOKENS
    auto packed_args = MakePackedUnknownArgs(std::forward<Args>(args)...);
    auto args_as_str = Join(packed_args.slots, packed_args.kNumSlots, packed_args.type_bits);
    pdp_trace("Request, token={}: {}({})", token, method, args_as_str.GetSlice());
#endif
    RpcBuilder builder(token, method);
    builder.OpenShortArray();
    (builder.Add(std::forward<Args>(args)), ...);
    builder.CloseShortArray();

    auto [data, size] = builder.Finish();
    SendBytes(data, size);
    return token++;
  }

  template <typename... Args>
  void BeginRpcRequest(RpcBuilder &builder, const StringSlice &method, Args &&...args) {
#if PDP_TRACE_RPC_TOKENS
    auto packed_args = MakePackedUnknownArgs(std::forward<Args>(args)...);
    auto args_as_str = Join(packed_args.slots, packed_args.kNumSlots, packed_args.type_bits);
    pdp_trace("Request, token={}: {}({})", token, method, args_as_str.GetSlice());
#endif
    builder.Restart(token, method);
    builder.OpenShortArray();
    (builder.Add(std::forward<Args>(args)), ...);
    token++;
  }

  void EndRpcRequest(RpcBuilder &builder) {
    builder.CloseShortArray();
    auto [data, size] = builder.Finish();
    SendBytes(data, size);
  }

  // Convience RPC request methods for common API functions

  uint32_t CreateNamespace(const StringSlice &ns);
  uint32_t Bufname(int64_t buffer);

  // Read RPC response methods

  uint32_t PollResponseToken(Milliseconds timeout);
  bool ReadBoolResult();
  int64_t ReadIntegerResult();
  DynamicString ReadStringResult();
  uint32_t OpenArrayResult();
  // TODO
  void SkipResult();

  static constexpr uint32_t kInvalidToken = 0;

 private:
  void SendBytes(const void *bytes, size_t num_bytes);

  OutputDescriptor vim_input;
  ByteStream vim_output;
  uint32_t token;
};

}  // namespace pdp

#pragma once

#include "core/log.h"
#include "parser/rpc_builder.h"
#include "strings/byte_stream.h"
#include "strings/dynamic_string.h"
#include "strings/string_slice.h"

#include <initializer_list>

namespace pdp {

struct VimDriver {
  VimDriver(int input_fd, int output_fd);

  // Send RPC methods

  uint32_t NextToken() const;

  template <typename... Args>
  uint32_t SendRpcRequest(const StringSlice &method, Args &&...args) {
    static_assert((IsRpcV<std::decay_t<Args>> && ...));

#if PDP_TRACE_RPC_TOKENS
    pdp_trace("Request: Method={}, token={}", method, token);
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
    pdp_trace("Request: Method={}, token={}", method, token);
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

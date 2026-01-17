#pragma once

#include "core/log.h"
#include "parser/rpc_builder.h"
#include "strings/byte_stream.h"
#include "strings/dynamic_string.h"
#include "strings/string_slice.h"

#include <initializer_list>

namespace pdp {

struct VimController {
  VimController(int input_fd, int output_fd);

  void ShowNormal(const StringSlice &msg);
  void ShowWarning(const StringSlice &msg);
  void ShowError(const StringSlice &msg);
  void ShowMessage(const StringSlice &msg, const StringSlice &hl);
  void ShowMessage(std::initializer_list<StringSlice> msg, std::initializer_list<StringSlice> hl);

  uint32_t NextToken() const;

  uint32_t CreateNamespace(const StringSlice &ns);
  uint32_t Bufname(int64_t buffer);
  // uint32_t SetExtmark(int bufnr, int ns, int line, int col, )

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

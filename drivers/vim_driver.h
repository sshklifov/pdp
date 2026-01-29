#pragma once

#include "core/log.h"
#include "parser/rpc_builder.h"
#include "strings/byte_stream.h"
#include "strings/fixed_string.h"
#include "strings/string_slice.h"

namespace pdp {

FixedString Join(pdp::PackedValue *slots, uint64_t num_slots, uint64_t type_bits);

struct VimRpcEvent {
  enum Type { kNone, kResponse, kNotify };

  VimRpcEvent(Type t) : type(static_cast<uint32_t>(t)) {}
  VimRpcEvent(uint32_t t) : type(kResponse), token(t) {}

  operator bool() const { return type != kNone; }

  bool IsNotify() const { return type == kNotify; }

  bool IsResponse() const { return type == kResponse; }

  uint32_t GetToken() const {
    pdp_assert(type == kResponse);
    return token;
  }

 private:
  uint32_t type;
  uint32_t token;
};

struct VimDriver {
  VimDriver(int input_fd, int output_fd);

  // Polling API

  VimRpcEvent PollRpcEvent();

  int GetDescriptor() const;

  // Send RPC methods

  uint32_t NextRequestToken() const;

  template <typename... Args>
  uint32_t SendRpcRequest(const StringSlice &method, Args &&...args) {
    static_assert((IsRpcV<std::decay_t<Args>> && ...));

#if PDP_TRACE_RPC_TOKENS
    auto packed_args = MakePackedUnknownArgs(std::forward<Args>(args)...);
    auto args_as_str = Join(packed_args.slots, packed_args.kNumSlots, packed_args.type_bits);
    pdp_trace("Request, token={}: {}({})", token, method, args_as_str.ToSlice());
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
  uint32_t BeginRpcRequest(RpcBuilder &builder, const StringSlice &method, Args &&...args) {
#if PDP_TRACE_RPC_TOKENS
    auto packed_args = MakePackedUnknownArgs(std::forward<Args>(args)...);
    auto args_as_str = Join(packed_args.slots, packed_args.kNumSlots, packed_args.type_bits);
    pdp_trace("Request, token={}: {}({})", token, method, args_as_str.ToSlice());
#endif
    builder.Restart(token, method);
    builder.OpenShortArray();
    (builder.Add(std::forward<Args>(args)), ...);
    return token++;
  }

  void EndRpcRequest(RpcBuilder &builder) {
    builder.CloseShortArray();
    auto [data, size] = builder.Finish();
    SendBytes(data, size);
  }

  // Read RPC response methods

  bool ReadBool();
  int64_t ReadInteger();
  FixedString ReadString();
  uint32_t OpenArray();
  void SkipResult();

 private:
  void SendBytes(const void *bytes, size_t num_bytes);

  OutputDescriptor vim_input;
  ByteStream vim_output;
  uint32_t token;
};

}  // namespace pdp

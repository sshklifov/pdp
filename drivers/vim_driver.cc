#include "vim_driver.h"

#include "parser/rpc_parser.h"

namespace pdp {

DynamicString Join(pdp::PackedValue *args, uint64_t num_slots, uint64_t type_bits) {
  StringSlice sep = ", ";
  const size_t bytes = RunEstimator(args, type_bits) + num_slots * sep.Size();
  DynamicString res;
  impl::_InPlaceStringInit string_init(res);
  char *s = string_init(bytes);

  Formatter fmt(s, s + bytes);
  while (type_bits) {
    size_t num_slots_used = fmt.AppendPackedValueUnchecked(args, type_bits);
    args += num_slots_used;
    type_bits >>= 4;
    if (PDP_LIKELY(type_bits)) {
      fmt.AppendUnchecked(sep);
    }
  }
  return res;
}

VimDriver::VimDriver(int input_fd, int output_fd)
    : vim_input(input_fd), vim_output(output_fd), token(1) {}

uint32_t VimDriver::NextToken() const { return token; }

uint32_t VimDriver::CreateNamespace(const StringSlice &ns) {
  return SendRpcRequest("nvim_create_namespace", ns);
}

uint32_t VimDriver::Bufname(int64_t buffer) { return SendRpcRequest("nvim_buf_get_name", buffer); }

void VimDriver::SendBytes(const void *bytes, size_t num_bytes) {
  bool success = vim_input.WriteExactly(bytes, num_bytes, Milliseconds(1000));
  if (PDP_UNLIKELY(!success)) {
    PDP_UNREACHABLE("Failed to send RPC request to VIM!");
  }
}

bool VimDriver::ReadBoolResult() { return ReadRpcBoolean(vim_output); }

int64_t VimDriver::ReadIntegerResult() { return ReadRpcInteger(vim_output); }

DynamicString VimDriver::ReadStringResult() { return ReadRpcString(vim_output); }

uint32_t VimDriver::OpenArrayResult() { return ReadRpcArrayLength(vim_output); }

void VimDriver::SkipResult() { return SkipRpcValue(vim_output); }

uint32_t VimDriver::PollResponseToken(Milliseconds timeout) {
  const bool has_bytes = vim_output.WaitForBytes(timeout);
  if (has_bytes) {
    ExpectRpcArrayWithLength(vim_output, 4);
    ExpectRpcInteger(vim_output, 1);
    int64_t token = ReadRpcInteger(vim_output);
    // TODO: SUS: why the fuck can I get a result and an error? TEST this.
    PrintRpcError(token, vim_output);
    return static_cast<uint32_t>(token);
  } else {
    return kInvalidToken;
  }
}

}  // namespace pdp

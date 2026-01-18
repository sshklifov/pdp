#include "vim_driver.h"

#include "parser/rpc_parser.h"

namespace pdp {

VimDriver::VimDriver(int input_fd, int output_fd)
    : vim_input(input_fd), vim_output(output_fd), token(1) {}

void VimDriver::ShowNormal(const StringSlice &msg) {
  SendRpcRequest("nvim_buf_set_lines", 0, -1, -1, true, msg);
}

void VimDriver::ShowWarning(const StringSlice &msg) { ShowMessage(msg, "WarningMsg"); }

void VimDriver::ShowError(const StringSlice &msg) { ShowMessage(msg, "ErrorMsg"); }

void VimDriver::ShowMessage(const StringSlice &msg, const StringSlice &hl) {
  ShowMessage({msg}, {hl});
}

void VimDriver::ShowMessage(std::initializer_list<StringSlice> msg,
                                std::initializer_list<StringSlice> hl) {
  // TODO
  (void)msg;
  (void)hl;
}

uint32_t VimDriver::NextToken() const { return token; }

uint32_t VimDriver::CreateNamespace(const StringSlice &ns) {
  return SendRpcRequest("nvim_create_namespace", ns);
}

uint32_t VimDriver::Bufname(int64_t buffer) {
  return SendRpcRequest("nvim_buf_get_name", buffer);
}

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

void VimDriver::SkipResult() { return SkipRpcError(vim_output); }

uint32_t VimDriver::PollResponseToken(Milliseconds timeout) {
  const bool has_bytes = vim_output.WaitForBytes(timeout);
  if (has_bytes) {
    ExpectRpcArrayWithLength(vim_output, 4);
    ExpectRpcInteger(vim_output, 1);
    int64_t token = ReadRpcInteger(vim_output);
    SkipRpcError(vim_output);
    return static_cast<uint32_t>(token);
  } else {
    return kInvalidToken;
  }
}

}  // namespace pdp

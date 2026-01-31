#include "vim_driver.h"

#include "parser/rpc_parser.h"

namespace pdp {

FixedString Join(pdp::PackedValue *args, uint64_t num_slots, uint64_t type_bits) {
  StringSlice sep = ", ";
  const size_t bytes = RunEstimator(args, type_bits) + num_slots * sep.Size() + 1;
  StringBuffer buffer(bytes);

  Formatter fmt(buffer.Get(), buffer.Get() + bytes);
  while (type_bits) {
    size_t num_slots_used = fmt.AppendPackedValueUnchecked(args, type_bits);
    args += num_slots_used;
    type_bits >>= 4;
    if (PDP_LIKELY(type_bits)) {
      fmt.AppendUnchecked(sep);
    }
  }

  auto length = fmt.End() - buffer.Get();
  buffer.Get()[length] = '\0';
  return FixedString(std::move(buffer), length);
}

VimDriver::VimDriver(int input_fd, int output_fd)
    : vim_input(input_fd), vim_output(output_fd), token(1) {}

int VimDriver::GetDescriptor() const { return vim_output.GetDescriptor(); }

uint32_t VimDriver::NextRequestToken() const { return token; }

void VimDriver::SendBytes(const void *bytes, size_t num_bytes) {
  bool success = vim_input.WriteExactly(bytes, num_bytes, Milliseconds(1000));
  if (PDP_UNLIKELY(!success)) {
    PDP_UNREACHABLE("Failed to send RPC request to VIM!");
  }
}

bool VimDriver::ReadBool() { return ReadRpcBoolean(vim_output); }

int64_t VimDriver::ReadInteger() { return ReadRpcInteger(vim_output); }

FixedString VimDriver::ReadString() { return ReadRpcString(vim_output); }

uint32_t VimDriver::OpenArray() { return ReadRpcArrayLength(vim_output); }

void VimDriver::SkipResult() { return SkipRpcValue(vim_output); }

VimRpcEvent VimDriver::PollRpcEvent() {
  const bool has_bytes = vim_output.PollBytes();
  if (has_bytes) {
    auto length = ReadRpcArrayLength(vim_output);
    auto type = ReadRpcInteger(vim_output);
    if (PDP_LIKELY(length == 4 && type == 1)) {
      int64_t token = ReadRpcInteger(vim_output);
      // TODO: SUS: why the fuck can I get a result and an error? TEST this.
      PrintRpcError(token, vim_output);
      return VimRpcEvent(token);
    } else if (PDP_LIKELY(length == 3 && type == 2)) {
      return VimRpcEvent(VimRpcEvent::kNotify);
    } else {
      PDP_FMT_UNREACHABLE("Unknown Vim RPC event type, length={} type={}", length, type);
    }
  } else {
    return VimRpcEvent(VimRpcEvent::kNone);
  }
}

}  // namespace pdp

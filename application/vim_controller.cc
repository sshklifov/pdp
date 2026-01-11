#include "vim_controller.h"

#include "core/log.h"
#include "parser/rpc_builder.h"
#include "parser/rpc_parser.h"

namespace pdp {

static size_t RunEstimator(std::initializer_list<StringSlice> ilist) {
  EstimateSize estimator;
  size_t res = 0;
  for (const auto &item : ilist) {
    res += estimator(item);
  }
  return res;
}

VimController::VimController(int input_fd, int output_fd)
    : vim_input(input_fd), vim_output(output_fd) {}

void VimController::ShowNormal(const StringSlice &msg) { ShowMessage(msg, "Normal"); }

void VimController::ShowWarning(const StringSlice &msg) { ShowMessage(msg, "WarningMsg"); }

void VimController::ShowError(const StringSlice &msg) { ShowMessage(msg, "ErrorMsg"); }

void VimController::ShowMessage(const StringSlice &msg, const StringSlice &hl) {
  ShowMessage({msg}, {hl});
}

void VimController::ShowMessage(std::initializer_list<StringSlice> msg,
                                std::initializer_list<StringSlice> hl) {
  RpcBuilder builder(1, "nvim_buf_set_lines");
  {
    auto params = builder.AddArray();
    builder.AddInteger(0);
    builder.AddInteger(-1);
    builder.AddInteger(-1);
    builder.AddBoolean(true);
    {
      auto lines = builder.AddArray();
      for (const auto &line : msg) {
        builder.AddString(line);
      }
    }
  }

  (void)hl;  // TODO
  auto [data, size] = builder.Finish();
  SendRpc(data, size);
}

bool VimController::SendRpc(const void *bytes, size_t num_bytes) {
  bool success = vim_input.WriteExactly(bytes, num_bytes, Milliseconds(1000));
  if (PDP_UNLIKELY(!success)) {
    pdp_warning("Failed to send RPC request to VIM!");
  }
  return success;
}

bool VimController::Poll(Milliseconds timeout) {
  bool has_data = vim_output.WaitForInput(timeout);
  return has_data;
  if (PDP_UNLIKELY(has_data)) {
    // Rpc
    // TODO RPC parser cannot own the file descriptor! 
    // RpcChunkArrayPass rpc_parser();
  }
}

}  // namespace pdp

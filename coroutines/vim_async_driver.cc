#include "vim_async_driver.h"

namespace pdp {

static size_t TotalStringBytes(std::initializer_list<StringSlice> ilist) {
  size_t res = 0;
  for (const auto &str : ilist) {
    res += str.Size();
  }
  return res;
}

VimAsyncDriver::VimAsyncDriver(int vim_input_fd, int vim_output_fd)
    : vim_driver(vim_input_fd, vim_output_fd) {
  InitializeNs();
  InitializeBuffers();
}

void VimAsyncDriver::RegisterForPoll(PollTable &table) {
  table.Register(vim_driver.GetDescriptor());
}

void VimAsyncDriver::OnPollResults(PollTable &table) {
  if (table.HasInputEventsUnchecked(vim_driver.GetDescriptor())) {
    Drain();
  }
}

void VimAsyncDriver::Drain() {
  uint32_t token = vim_driver.PollResponseToken();
  while (token != vim_driver.kInvalidToken) {
#if PDP_TRACE_RPC_TOKENS
    pdp_trace("Response: token={}", token);
#endif
    const bool token_handled = suspended_handlers.Resume(token);
    if (!token_handled) {
      vim_driver.SkipResult();
#if PDP_TRACE_RPC_TOKENS
      pdp_trace("Skipped: token={}", token);
#endif
    }
    token = vim_driver.PollResponseToken();
  }
}

IntegerArrayRpcAwaiter VimAsyncDriver::PromiseBufferList() {
  uint32_t list_token = vim_driver.SendRpcRequest("nvim_list_bufs");
  return IntegerArrayRpcAwaiter(this, list_token);
}

void VimAsyncDriver::ShowNormal(const StringSlice &msg) {
  pdp_assert(!msg.Empty());
  auto bufnr = buffers[kPromptBuf];
  vim_driver.SendRpcRequest("nvim_buf_set_lines", bufnr, num_prompt_lines, num_prompt_lines, true,
                            std::initializer_list<StringSlice>{msg});
  num_prompt_lines++;
}

void VimAsyncDriver::ShowPacked(const StringSlice &fmt, PackedValue *args, uint64_t type_bits) {
  StringBuilder builder;
  builder.AppendPack(fmt, args, type_bits);
  ShowNormal(builder.ToSlice());
}

void VimAsyncDriver::ShowWarning(const StringSlice &msg) { ShowMessage(msg, "WarningMsg"); }

void VimAsyncDriver::ShowError(const StringSlice &msg) { ShowMessage(msg, "ErrorMsg"); }

void VimAsyncDriver::ShowMessage(const StringSlice &msg, const StringSlice &hl) {
  ShowMessage(std::initializer_list<StringSlice>{msg}, std::initializer_list<StringSlice>{hl});
}

void VimAsyncDriver::ShowMessage(std::initializer_list<StringSlice> ilist_msg,
                                 std::initializer_list<StringSlice> ilist_hl) {
  pdp_assert(ilist_msg.size() == ilist_hl.size());

  // Message

  auto bufnr = buffers[kPromptBuf];
  RpcBuilder builder;
  vim_driver.BeginRpcRequest(builder, "nvim_buf_set_lines", bufnr, num_prompt_lines,
                             num_prompt_lines, true);
  builder.OpenShortArray();
  char *msg_out = builder.AddUninitializedString(TotalStringBytes(ilist_msg));
  builder.CloseShortArray();

  for (const auto &msg : ilist_msg) {
    memcpy(msg_out, msg.Data(), msg.Size());
    msg_out += msg.Size();
  }
  vim_driver.EndRpcRequest(builder);

  // Highlight

  int start_col = 0;
  for (size_t i = 0; i < ilist_hl.size(); ++i) {
    int end_col = start_col + ilist_msg.begin()[i].Size();

    vim_driver.BeginRpcRequest(builder, "nvim_buf_set_extmark", bufnr, namespaces[kHighlightNs],
                               num_prompt_lines, start_col);
    builder.OpenShortMap();
    builder.AddMapItem("end_col", end_col);
    builder.AddMapItem("hl_group", ilist_hl.begin()[i]);
    builder.CloseShortMap();
    vim_driver.EndRpcRequest(builder);

    start_col = end_col;
  }

  vim_driver.SendRpcRequest("nvim_buf_set_option", bufnr, "modified", false);
  num_prompt_lines++;
}

HandlerCoroutine VimAsyncDriver::InitializeNs() {
  uint32_t start_token = vim_driver.CreateNamespace("PromptDebugHighlight");
  vim_driver.CreateNamespace("PromptDebugPC");
  vim_driver.CreateNamespace("PromptDebugRegister");
  vim_driver.CreateNamespace("PromptDebugPrompt");
  vim_driver.CreateNamespace("PromptDebugConcealVar");
  vim_driver.CreateNamespace("PromptDebugConcealJump");
  vim_driver.CreateNamespace("PromptDebugBreakpoint");
  pdp_assert(vim_driver.NextToken() - start_token == kTotalNs);

  namespaces[kHighlightNs] = co_await IntegerRpcAwaiter(this, start_token);
  namespaces[kProgramCounterNs] = co_await IntegerRpcAwaiter(this, start_token + 1);
  namespaces[kRegisterNs] = co_await IntegerRpcAwaiter(this, start_token + 2);
  namespaces[kPromptBufferNs] = co_await IntegerRpcAwaiter(this, start_token + 3);
  namespaces[kConcealVarNs] = co_await IntegerRpcAwaiter(this, start_token + 4);
  namespaces[kConcealJumpNs] = co_await IntegerRpcAwaiter(this, start_token + 5);
  namespaces[kBreakpointNs] = co_await IntegerRpcAwaiter(this, start_token + 6);
}

HandlerCoroutine VimAsyncDriver::InitializeBuffers() {
  const Vector<int64_t> list = co_await PromiseBufferList();
  uint32_t token = vim_driver.NextToken();
  for (size_t i = 0; i < list.Size(); ++i) {
    vim_driver.Bufname(list[i]);
  }
  memset(buffers, -1, sizeof(buffers));

  StringSlice names[kTotalBufs];
  names[kCaptureBuf] = "Gdb capture";
  names[kAsmBuf] = "Gdb disas";
  names[kPromptBuf] = "Gdb prompt";
  names[kIoBuf] = "Gdb i/o";

  for (size_t i = 0; i < list.Size(); ++i) {
    DynamicString dynamic_str = co_await StringRpcAwaiter(this, token + i);
    StringSlice name = dynamic_str.ToSlice();
    if (name.Size() >= 1) {
      switch (name[name.Size() - 1]) {
        case 'e':
          if (PDP_LIKELY(name.EndsWith(names[kCaptureBuf]))) {
            buffers[kCaptureBuf] = list[i];
          }
          break;
        case 's':
          if (PDP_LIKELY(name.EndsWith(names[kAsmBuf]))) {
            buffers[kAsmBuf] = list[i];
          }
          break;
        case 't':
          if (PDP_LIKELY(name.EndsWith(names[kPromptBuf]))) {
            buffers[kPromptBuf] = list[i];
          }
          break;
        case 'o':
          if (PDP_LIKELY(name.EndsWith(names[kIoBuf]))) {
            buffers[kIoBuf] = list[i];
          }
          break;
      }
    }
  }

  token = vim_driver.NextToken();
  for (size_t i = 0; i < kTotalBufs; ++i) {
    if (buffers[i] < 0) {
      vim_driver.SendRpcRequest("nvim_create_buf", true, false);
    }
  }
  for (size_t i = 0; i < kTotalBufs; ++i) {
    if (buffers[i] < 0) {
      buffers[i] = co_await IntegerRpcAwaiter(this, token);
      vim_driver.SendRpcRequest("nvim_buf_set_name", buffers[i], names[i]);
      ++token;
    }
  }

  vim_driver.SendRpcRequest("nvim_buf_set_lines", buffers[kPromptBuf], 0, -1, false,
                            std::initializer_list<StringSlice>{});
  vim_driver.SendRpcRequest("nvim_buf_set_option", buffers[kPromptBuf], "modified", false);
  num_prompt_lines = 0;
}

}  // namespace pdp

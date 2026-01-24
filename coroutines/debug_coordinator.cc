#include "debug_coordinator.h"

namespace pdp {

static size_t TotalStringBytes(std::initializer_list<StringSlice> ilist) {
  size_t res = 0;
  for (const auto &str : ilist) {
    res += str.Size();
  }
  return res;
}

DebugCoordinator::DebugCoordinator(const StringSlice &host, int vim_input_fd, int vim_output_fd,
                                   ChildReaper &reaper)
    : ssh_driver(nullptr), gdb_async(reaper), vim_driver(vim_input_fd, vim_output_fd) {
  if (!host.Empty()) {
    ssh_driver = Allocate<SshDriver>(allocator, 1);
    new (ssh_driver) SshDriver(host, reaper);
  }

  InitializeNs();
  InitializeBuffers();
}

DebugCoordinator::~DebugCoordinator() {
  if (ssh_driver) {
    Deallocate<SshDriver>(allocator, ssh_driver);
  }
}

HandlerCoroutine DebugCoordinator::InitializeNs() {
  uint32_t start_token = vim_driver.CreateNamespace("PromptDebugHighlight");
  vim_driver.CreateNamespace("PromptDebugPC");
  vim_driver.CreateNamespace("PromptDebugRegister");
  vim_driver.CreateNamespace("PromptDebugPrompt");
  vim_driver.CreateNamespace("PromptDebugConcealVar");
  vim_driver.CreateNamespace("PromptDebugConcealJump");
  vim_driver.CreateNamespace("PromptDebugBreakpoint");
  pdp_assert(vim_driver.NextToken() - start_token == kTotalNs);

  session_data.namespaces[kHighlightNs] = co_await IntegerRpcAwaiter(this, start_token);
  session_data.namespaces[kProgramCounterNs] = co_await IntegerRpcAwaiter(this, start_token + 1);
  session_data.namespaces[kRegisterNs] = co_await IntegerRpcAwaiter(this, start_token + 2);
  session_data.namespaces[kPromptBufferNs] = co_await IntegerRpcAwaiter(this, start_token + 3);
  session_data.namespaces[kConcealVarNs] = co_await IntegerRpcAwaiter(this, start_token + 4);
  session_data.namespaces[kConcealJumpNs] = co_await IntegerRpcAwaiter(this, start_token + 5);
  session_data.namespaces[kBreakpointNs] = co_await IntegerRpcAwaiter(this, start_token + 6);
}

HandlerCoroutine DebugCoordinator::InitializeBuffers() {
  Vector<int64_t> buffers = co_await RpcListBuffers();
  uint32_t token = vim_driver.NextToken();
  for (size_t i = 0; i < buffers.Size(); ++i) {
    vim_driver.Bufname(buffers[i]);
  }
  session_data.buffers[kCaptureBuf] = -1;
  session_data.buffers[kAsmBuf] = -1;
  session_data.buffers[kPromptBuf] = -1;
  session_data.buffers[kIoBuf] = -1;

  StringSlice names[kTotalBufs];
  names[kCaptureBuf] = "Gdb capture";
  names[kAsmBuf] = "Gdb disas";
  names[kPromptBuf] = "Gdb prompt";
  names[kIoBuf] = "Gdb i/o";

  for (size_t i = 0; i < buffers.Size(); ++i) {
    auto str = co_await StringRpcAwaiter(this, token + i);
    StringSlice name = str.ToSlice();
    if (name.Size() >= 1) {
      switch (name[name.Size() - 1]) {
        case 'e':
          if (PDP_LIKELY(name.EndsWith(names[kCaptureBuf]))) {
            session_data.buffers[kCaptureBuf] = buffers[i];
          }
          break;
        case 's':
          if (PDP_LIKELY(name.EndsWith(names[kAsmBuf]))) {
            session_data.buffers[kAsmBuf] = buffers[i];
          }
          break;
        case 't':
          if (PDP_LIKELY(name.EndsWith(names[kPromptBuf]))) {
            session_data.buffers[kPromptBuf] = buffers[i];
          }
          break;
        case 'o':
          if (PDP_LIKELY(name.EndsWith(names[kIoBuf]))) {
            session_data.buffers[kIoBuf] = buffers[i];
          }
          break;
      }
    }
  }

  token = vim_driver.NextToken();
  for (size_t i = 0; i < kTotalBufs; ++i) {
    if (session_data.buffers[i] < 0) {
      vim_driver.SendRpcRequest("nvim_create_buf", true, false);
    }
  }
  // TODO wtf naming returned by vim...

  for (size_t i = 0; i < kTotalBufs; ++i) {
    if (session_data.buffers[i] < 0) {
      session_data.buffers[i] = co_await IntegerRpcAwaiter(this, token);
      vim_driver.SendRpcRequest("nvim_buf_set_name", session_data.buffers[i], names[i]);
      ++token;
    }
  }

  vim_driver.SendRpcRequest("nvim_buf_set_lines", session_data.buffers[kPromptBuf], 0, -1, false,
                            std::initializer_list<StringSlice>{});
  vim_driver.SendRpcRequest("nvim_buf_set_option", session_data.buffers[kPromptBuf], "modified",
                            false);
  session_data.num_lines_written = 0;
}

IntegerArrayRpcAwaiter DebugCoordinator::RpcListBuffers() {
  uint32_t list_token = vim_driver.SendRpcRequest("nvim_list_bufs");
  return IntegerArrayRpcAwaiter(this, list_token);
}

void DebugCoordinator::RpcClearNamespace(int bufnr, int ns) {
  vim_driver.SendRpcRequest("nvim_buf_clear_namespace", bufnr, ns, 0, -1);
}

void DebugCoordinator::RpcShowNormal(const StringSlice &msg) {
  pdp_assert(!msg.Empty());

  auto old_line_count = session_data.num_lines_written;
  auto bufnr = session_data.buffers[kPromptBuf];
  vim_driver.SendRpcRequest("nvim_buf_set_lines", bufnr, old_line_count, old_line_count, true,
                            std::initializer_list<StringSlice>{msg});
  session_data.num_lines_written++;
}

void DebugCoordinator::RpcShowPacked(const StringSlice &fmt, PackedValue *args,
                                     uint64_t type_bits) {
  StringBuilder builder;
  builder.AppendPack(fmt, args, type_bits);
  RpcShowNormal(builder.ToSlice());
}

void DebugCoordinator::RpcShowWarning(const StringSlice &msg) { RpcShowMessage(msg, "WarningMsg"); }

void DebugCoordinator::RpcShowError(const StringSlice &msg) { RpcShowMessage(msg, "ErrorMsg"); }

void DebugCoordinator::RpcShowMessage(const StringSlice &msg, const StringSlice &hl) {
  RpcShowMessage(std::initializer_list<StringSlice>{msg}, std::initializer_list<StringSlice>{hl});
}

void DebugCoordinator::RpcShowMessage(std::initializer_list<StringSlice> ilist_msg,
                                      std::initializer_list<StringSlice> ilist_hl) {
  pdp_assert(ilist_msg.size() == ilist_hl.size());

  auto old_line_count = session_data.num_lines_written;
  auto bufnr = session_data.buffers[kPromptBuf];

  RpcBuilder builder;
  vim_driver.BeginRpcRequest(builder, "nvim_buf_set_lines", bufnr, old_line_count, old_line_count,
                             true);
  builder.OpenShortArray();
  char *msg_out = builder.AddUninitializedString(TotalStringBytes(ilist_msg));
  builder.CloseShortArray();

  for (const auto &msg : ilist_msg) {
    memcpy(msg_out, msg.Data(), msg.Size());
    msg_out += msg.Size();
  }
  vim_driver.EndRpcRequest(builder);

  session_data.num_lines_written++;

  int start_col = 0;
  for (size_t i = 0; i < ilist_hl.size(); ++i) {
    int end_col = start_col + ilist_msg.begin()[i].Size();

    vim_driver.BeginRpcRequest(builder, "nvim_buf_set_extmark", bufnr,
                               session_data.namespaces[kHighlightNs], old_line_count, start_col);
    builder.OpenShortMap();
    builder.AddMapItem("end_col", end_col);
    builder.AddMapItem("hl_group", ilist_hl.begin()[i]);
    builder.CloseShortMap();
    vim_driver.EndRpcRequest(builder);

    start_col = end_col;
  }

  vim_driver.SendRpcRequest("nvim_buf_set_option", bufnr, "modified", false);
}

int DebugCoordinator::GetExeTimetamp() {
  if (!PDP_UNLIKELY(session_data.HasExeTimestamp())) {
    // TODO
  }
  return session_data.GetExeTimestamp();
}

void DebugCoordinator::RegisterForPoll(PollTable &table) {
  gdb_async.RegisterForPoll(table);
  table.Register(vim_driver.GetDescriptor());
  if (ssh_driver) {
    ssh_driver->RegisterForPoll(table);
  }
}

void DebugCoordinator::OnPollResults(PollTable &table) {
  gdb_async.OnPollResults(table);
  if (table.HasInputEventsUnchecked(vim_driver.GetDescriptor())) {
    DrainVim();
  }
  if (ssh_driver) {
    ssh_driver->OnPollResults(table);
  }
}

void DebugCoordinator::DrainVim() {
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

bool DebugCoordinator::IsIdle() const { return suspended_handlers.Empty(); }

void DebugCoordinator::PrintActivity() const {
  if (PDP_LIKELY(!suspended_handlers.Empty())) {
    suspended_handlers.PrintSuspendedTokens();
  }
}

BooleanRpcAwaiter DebugCoordinator::RpcBufExists(int bufnr) {
  uint32_t token = vim_driver.SendRpcRequest("nvim_buf_is_valid", bufnr);
  return BooleanRpcAwaiter(this, token);
}

}  // namespace pdp

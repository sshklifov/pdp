#include "debug_coordinator.h"

#include "handlers/handle_stream.h"

#include "parser/mi_parser.h"

namespace pdp {

static size_t TotalStringBytes(std::initializer_list<StringSlice> ilist) {
  size_t res = 0;
  for (const auto &str : ilist) {
    res += str.Size();
  }
  return res;
}

DebugCoordinator::DebugCoordinator(int vim_input_fd, int vim_output_fd)
    : vim_controller(vim_input_fd, vim_output_fd) {
  gdb_driver.Start();

  InitializeNs();
  InitializeBuffers();
}

HandlerCoroutine DebugCoordinator::InitializeNs() {
  uint32_t start_token = vim_controller.CreateNamespace("PromptDebugHighlight");
  vim_controller.CreateNamespace("PromptDebugPC");
  vim_controller.CreateNamespace("PromptDebugRegister");
  vim_controller.CreateNamespace("PromptDebugPrompt");
  vim_controller.CreateNamespace("PromptDebugConcealVar");
  vim_controller.CreateNamespace("PromptDebugConcealJump");
  vim_controller.CreateNamespace("PromptDebugBreakpoint");
  pdp_assert(vim_controller.NextToken() - start_token == kTotalNs);

  session_data.namespaces[kHighlightNs] = co_await IntegerRpcAwaiter(this, start_token);
  session_data.namespaces[kProgramCounterNs] = co_await IntegerRpcAwaiter(this, start_token + 1);
  session_data.namespaces[kRegisterNs] = co_await IntegerRpcAwaiter(this, start_token + 2);
  session_data.namespaces[kPromptBufferNs] = co_await IntegerRpcAwaiter(this, start_token + 3);
  session_data.namespaces[kConcealVarNs] = co_await IntegerRpcAwaiter(this, start_token + 4);
  session_data.namespaces[kConcealJumpNs] = co_await IntegerRpcAwaiter(this, start_token + 5);
  session_data.namespaces[kBreakpointNs] = co_await IntegerRpcAwaiter(this, start_token + 6);
}

HandlerCoroutine DebugCoordinator::InitializeBuffers() {
  Vector<int64_t> buffers = co_await ListBuffers();
  uint32_t token = vim_controller.NextToken();
  for (size_t i = 0; i < buffers.Size(); ++i) {
    vim_controller.Bufname(buffers[i]);
  }
  session_data.buffers[kCaptureBuf] = -1;
  session_data.buffers[kAsmBuf] = -1;
  session_data.buffers[kPromptBuf] = -1;
  session_data.buffers[kIoBuf] = -1;

  const char *names[kTotalBufs];
  names[kCaptureBuf] = "Gdb capture";
  names[kAsmBuf] = "Gdb disas";
  names[kPromptBuf] = "Gdb prompt";
  names[kIoBuf] = "Gdb i/o";

  for (size_t i = 0; i < buffers.Size(); ++i) {
    auto str = co_await StringRpcAwaiter(this, token + i);
    StringSlice name = str.GetSlice();
    StringSlice prefix("Gdb ");
    if (name.Size() >= prefix.Size() + 1 && name.MemCmp(prefix) == 0) {
      name.DropLeft(prefix.Size());
      switch (name[0]) {
        case 'c':
          if (PDP_LIKELY(name == "capture")) {
            session_data.buffers[kCaptureBuf] = buffers[i];
          }
          break;
        case 'd':
          if (PDP_LIKELY(name == "disas")) {
            session_data.buffers[kAsmBuf] = buffers[i];
          }
          break;
        case 'p':
          if (PDP_LIKELY(name == "prompt")) {
            session_data.buffers[kPromptBuf] = buffers[i];
          }
          break;
        case 'i':
          if (PDP_LIKELY(name == "i/o")) {
            session_data.buffers[kIoBuf] = buffers[i];
          }
          break;
      }
    }
  }

  token = vim_controller.NextToken();
  for (size_t i = 0; i < kTotalBufs; ++i) {
    if (session_data.buffers[i] < 0) {
      vim_controller.SendRpcRequest("nvim_create_buf", true, false);
    }
  }
  // TODO wtf naming returned by vim...

  for (size_t i = 0; i < kTotalBufs; ++i) {
    if (session_data.buffers[i] < 0) {
      session_data.buffers[i] = co_await IntegerRpcAwaiter(this, token);
      vim_controller.SendRpcRequest("nvim_buf_set_name", session_data.buffers[i], names[i]);
      ++token;
    }
  }

  vim_controller.SendRpcRequest("nvim_buf_set_lines", session_data.buffers[kPromptBuf], 0, -1,
                                false, std::initializer_list<StringSlice>{});
  session_data.num_lines_written = 0;
}

void DebugCoordinator::PollGdb(Milliseconds timeout) {
  GdbRecord record;
  RecordKind kind = gdb_driver.Poll(timeout, &record);
  if (PDP_UNLIKELY(kind != RecordKind::kNone)) {
    if (kind == RecordKind::kStream) {
      HandleStream(this, record.stream.message);
    } else {
      MiFirstPass first_pass(record.result_or_async.results);
      if (PDP_UNLIKELY(!first_pass.Parse())) {
        pdp_error("Pass #1 failed on: {}", record.result_or_async.results);
        return;
      }
      MiSecondPass second_pass(record.result_or_async.results, first_pass);
      ScopedPtr<ExprBase> expr(second_pass.Parse());
      if (PDP_UNLIKELY(!expr)) {
        pdp_error("Pass #2 failed on: {}", record.result_or_async.results);
        return;
      }
      if (kind == RecordKind::kAsync) {
        HandleAsync(static_cast<GdbAsyncKind>(record.result_or_async.kind), std::move(expr));
      } else if (kind == RecordKind::kResult) {
        HandleResult(static_cast<GdbResultKind>(record.result_or_async.kind), std::move(expr));
      } else {
        pdp_assert(false);
      }
    }
  }
}

IntegerArrayRpcAwaiter DebugCoordinator::ListBuffers() {
  uint32_t list_token = vim_controller.SendRpcRequest("nvim_list_bufs");
  return IntegerArrayRpcAwaiter(this, list_token);
}

void DebugCoordinator::ShowNormal(const StringSlice &msg) {
  pdp_assert(!msg.Empty());

  auto old_line_count = session_data.num_lines_written;
  auto bufnr = session_data.buffers[kPromptBuf];
  vim_controller.SendRpcRequest("nvim_buf_set_lines", bufnr, old_line_count, old_line_count, true,
                                std::initializer_list<StringSlice>{msg});
  session_data.num_lines_written++;
}

void DebugCoordinator::ShowWarning(const StringSlice &msg) { ShowMessage(msg, "WarningMsg"); }

void DebugCoordinator::ShowError(const StringSlice &msg) { ShowMessage(msg, "ErrorMsg"); }

void DebugCoordinator::ShowMessage(const StringSlice &msg, const StringSlice &hl) {
  ShowMessage(std::initializer_list<StringSlice>{msg}, std::initializer_list<StringSlice>{hl});
}

void DebugCoordinator::ShowMessage(std::initializer_list<StringSlice> ilist_msg,
                                   std::initializer_list<StringSlice> ilist_hl) {
  pdp_assert(ilist_msg.size() == ilist_hl.size());

  auto old_line_count = session_data.num_lines_written;
  auto bufnr = session_data.buffers[kPromptBuf];

  RpcBuilder builder;
  vim_controller.BeginRpcRequest(builder, "nvim_buf_set_lines", bufnr, old_line_count,
                                 old_line_count, true);
  builder.OpenShortArray();
  char *msg_out = builder.AddUninitializedString(TotalStringBytes(ilist_msg));
  builder.CloseShortArray();

  for (const auto &msg : ilist_msg) {
    memcpy(msg_out, msg.Data(), msg.Size());
    msg_out += msg.Size();
  }
  vim_controller.EndRpcRequest(builder);

  session_data.num_lines_written++;

  int start_col = 0;
  for (size_t i = 0; i < ilist_hl.size(); ++i) {
    int end_col = start_col + ilist_msg.begin()[i].Size();

    vim_controller.BeginRpcRequest(builder, "nvim_buf_set_extmark", bufnr,
                                   session_data.namespaces[kHighlightNs], old_line_count,
                                   start_col);
    builder.OpenShortMap();
    builder.AddMapItem("end_col", end_col);
    builder.AddMapItem("hl_group", ilist_hl.begin()[i]);
    builder.CloseShortMap();
    vim_controller.EndRpcRequest(builder);

    start_col = end_col;
  }

  vim_controller.SendRpcRequest("nvim_buf_set_option", bufnr, "modified", false);
}

void DebugCoordinator::HandleResult(GdbResultKind kind, ScopedPtr<ExprBase> &&expr) {
  if (PDP_LIKELY(kind == GdbResultKind::kDone)) {
    // TODO
  } else if (PDP_LIKELY(kind == GdbResultKind::kError)) {
    // TODO
  }
}

void DebugCoordinator::HandleAsync(GdbAsyncKind kind, ScopedPtr<ExprBase> &&expr) {
  switch (kind) {
    case GdbAsyncKind::kThreadSelected: {
      // HandleThreadSelected h;
      // return h(this, std::move(expr));
    }
  }
}

void DebugCoordinator::PollVim(Milliseconds timeout) {
  uint32_t token = vim_controller.PollResponseToken(timeout);
  if (token != vim_controller.kInvalidToken) {
#if PDP_TRACE_RPC_TOKENS
    pdp_trace("Response: token={}", token);
#endif
    const bool rpc_payload_read = suspended_handlers.Resume(token);
    if (!rpc_payload_read) {
      vim_controller.SkipResult();
#if PDP_TRACE_RPC_TOKENS
      pdp_trace("Skipped: token={}", token);
#endif
    }
  }
}

void DebugCoordinator::ReachIdle(Milliseconds timeout) {
  Stopwatch stopwatch;
  auto next_wait = timeout;
  while (!suspended_handlers.Empty() && next_wait > 0_ms) {
    PollVim(next_wait > 5_ms ? next_wait : 5_ms);
    next_wait = timeout - stopwatch.ElapsedMilli();
  }
  if (PDP_UNLIKELY(!suspended_handlers.Empty())) {
    suspended_handlers.PrintSuspendedTokens();
    PDP_UNREACHABLE("Failed to process all coroutines within timeout");
  }
}

BooleanRpcAwaiter DebugCoordinator::BufExists(int64_t bufnr) {
  uint32_t token = vim_controller.SendRpcRequest("nvim_buf_is_valid", bufnr);
  return BooleanRpcAwaiter(this, token);
}

}  // namespace pdp

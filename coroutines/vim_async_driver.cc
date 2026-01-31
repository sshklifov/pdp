#include "vim_async_driver.h"

namespace pdp {

IntegerRpcAwaiter IntegerRpcQueue::NextAwaiter() {
  return IntegerRpcAwaiter(async_driver, token_begin++);
}

StringRpcAwaiter StringRpcQueue::NextAwaiter() {
  return StringRpcAwaiter(async_driver, token_begin++);
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
  VimRpcEvent event = vim_driver.PollRpcEvent();
  while (event) {
    if (PDP_LIKELY(event.IsResponse())) {
#if PDP_TRACE_RPC_TOKENS
      pdp_trace("Response: token={}", event.GetToken());
#endif
      const bool token_handled = suspended_handlers.Resume(event.GetToken());
      if (!token_handled) {
        vim_driver.SkipResult();
#if PDP_TRACE_RPC_TOKENS
        pdp_trace("Skipped: token={}", event.GetToken());
#endif
      }
    } else {
      pdp_assert(event.IsNotify());
      ReadNotifyEvent();
    }

    // Read the next event
    event = vim_driver.PollRpcEvent();
  }
}

void VimAsyncDriver::ReadNotifyEvent() {
  FixedString method = vim_driver.ReadString();
  auto elems = vim_driver.OpenArray();
  if (method == "pdp/buf_changed") {
    if (PDP_UNLIKELY(elems != 2)) {
      // TODO unreachable + critical error... NOT THAT HARD! EVERYWHERE....
      PDP_UNREACHABLE("Unexpected number of elements!");
    }
    auto bufnr = vim_driver.ReadInteger();
    auto name = vim_driver.ReadString();
    if (name.ToSlice().StartsWith('/') && FileReadable(name.Cstr())) {
      auto [it, _] = opened_buffers.Emplace(std::move(name));
      it->value = bufnr;
      OnNotifyNewBuffer(it->key.ToSlice(), it->value);
    }
  } else if (method == "pdp/buf_removed") {
    if (PDP_UNLIKELY(elems != 1)) {
      // TODO unreachable + critical error... NOT THAT HARD! EVERYWHERE....
      PDP_UNREACHABLE("Unexpected number of elements!");
    }
    auto name = vim_driver.ReadString();
    auto it = opened_buffers.Find(name.ToSlice());
    if (it != opened_buffers.End()) {
      opened_buffers.Erase(it);
    }
  } else {
    pdp_error("Unexpected notification: {}", method.ToSlice());
    PDP_UNREACHABLE("Unhandled notification");
  }
}

void VimAsyncDriver::OnNotifyNewBuffer(const StringSlice &fullname, int bufnr) {
  pdp_info("Triggered notify event fullname={} bufnr={}", fullname, bufnr);
#if 0
  auto it = pending_extmarks.Find(fullname);
  while (it != pending_extmarks.End()) {
    // TODO here vim_driver.Send
    // TODO change the whole vim async driver marks to use this path
  }
#endif
}

IntegerRpcQueue VimAsyncDriver::PrepareIntegerQueue() {
  return IntegerRpcQueue(this, vim_driver.NextRequestToken());
}

StringRpcQueue VimAsyncDriver::PrepareStringQueue() {
  return StringRpcQueue(this, vim_driver.NextRequestToken());
}

IntegerRpcAwaiter VimAsyncDriver::PromiseCreateBuffer() {
  auto token = vim_driver.SendRpcRequest("nvim_create_buf", true, false);
  return IntegerRpcAwaiter(this, token);
}

IntegerRpcAwaiter VimAsyncDriver::PromiseNamespace(const StringSlice &ns) {
  auto token = vim_driver.SendRpcRequest("nvim_create_namespace", ns);
  return IntegerRpcAwaiter(this, token);
}

StringRpcAwaiter VimAsyncDriver::PromiseBufferName(int64_t buffer) {
  auto token = vim_driver.SendRpcRequest("nvim_buf_get_name", buffer);
  return StringRpcAwaiter(this, token);
}

IntegerArrayRpcAwaiter VimAsyncDriver::PromiseBufferList() {
  uint32_t list_token = vim_driver.SendRpcRequest("nvim_list_bufs");
  return IntegerArrayRpcAwaiter(this, list_token);
}

IntegerRpcAwaiter VimAsyncDriver::PromiseBufferLineCount(int bufnr) {
  auto token = vim_driver.SendRpcRequest("nvim_buf_line_count", bufnr);
  return IntegerRpcAwaiter(this, token);
}

void VimAsyncDriver::DeleteBreakpointMark(const StringSlice &fullname, int extmark) {
  auto it = opened_buffers.Find(fullname);
  if (it != opened_buffers.End()) {
    auto bufnr = it->value;
    vim_driver.SendRpcRequest("nvim_buf_del_extmark", bufnr, namespaces[kBreakpointNs], extmark);
  }
}

#if 0
IntegerRpcAwaiter VimAsyncDriver::SetBreakpointMark(StringSlice mark, const StringSlice &fullname,
                                                    int lnum, int enabled) {
  if 
}

void VimAsyncDriver::SetBreakpointMark(StringSlice mark, int bufnr, int lnum, int enabled) {
  RpcBuilder builder;
  auto token = vim_driver.BeginRpcRequest(builder, "nvim_buf_set_extmark", bufnr,
                                          namespaces[kBreakpointNs], lnum - 1, 0);
  builder.OpenShortMap();
  builder.AddMapItem("sign_text", mark.Length() <= 2 ? mark : mark.Substr(2));
  builder.AddMapItem("sign_hl_group", enabled ? "debugBreakpoint" : "debugBreakpointDisabled");
  builder.CloseShortMap();
  vim_driver.EndRpcRequest(builder);
  return IntegerRpcAwaiter(this, token);
}
#endif

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

void VimAsyncDriver::ShowMessage(const MessageBuilder &message) {
  // Message

  auto bufnr = buffers[kPromptBuf];
  RpcBuilder builder;
  vim_driver.BeginRpcRequest(builder, "nvim_buf_set_lines", bufnr, num_prompt_lines,
                             num_prompt_lines, true);
  builder.OpenShortArray();
  builder.Add(message.GetJoinedMessage());
  builder.CloseShortArray();
  vim_driver.EndRpcRequest(builder);

  // Highlight

  size_t start_col = 0;
  for (const auto &[msg_len, hl] : message) {
    size_t end_col = start_col + msg_len;

    if (hl != "Normal") {
      vim_driver.BeginRpcRequest(builder, "nvim_buf_set_extmark", bufnr,
                                 namespaces[kPromptBufferNs], num_prompt_lines, start_col);
      builder.OpenShortMap();
      builder.AddMapItem("end_col", end_col);
      builder.AddMapItem("hl_group", hl);
      builder.CloseShortMap();
      vim_driver.EndRpcRequest(builder);
    }

    start_col = end_col;
  }

  vim_driver.SendRpcRequest("nvim_buf_set_option", bufnr, "modified", false);
  num_prompt_lines++;
}

void VimAsyncDriver::HighlightLastLine(const StringSlice &hl) {
  RpcBuilder builder;
  vim_driver.BeginRpcRequest(builder, "nvim_buf_set_extmark", buffers[kPromptBuf],
                             namespaces[kPromptBufferNs], num_prompt_lines - 1, 0);
  builder.OpenShortMap();
  builder.AddMapItem("line_hl_group", hl);
  builder.CloseShortMap();
  vim_driver.EndRpcRequest(builder);
}

void VimAsyncDriver::HighlightLastLine(int start_col, int end_col, const StringSlice &hl) {
  // TODO slight copy pasta
  RpcBuilder builder;
  vim_driver.BeginRpcRequest(builder, "nvim_buf_set_extmark", buffers[kPromptBuf],
                             namespaces[kPromptBufferNs], num_prompt_lines - 1, start_col);
  builder.OpenShortMap();
  builder.AddMapItem("end_col", end_col);
  builder.AddMapItem("hl_group", hl);
  builder.CloseShortMap();
  vim_driver.EndRpcRequest(builder);
}

Coroutine VimAsyncDriver::InitializeNs() {
  IntegerRpcQueue queue = PrepareIntegerQueue();
  PromiseNamespace("PromptDebugPC").Enqueue(queue);
  PromiseNamespace("PromptDebugPrompt").Enqueue(queue);
  PromiseNamespace("PromptDebugBreakpoint").Enqueue(queue);
  pdp_assert(queue.Size() == kTotalNs);

  namespaces[kProgramCounterNs] = co_await queue.NextAwaiter();
  namespaces[kPromptBufferNs] = co_await queue.NextAwaiter();
  namespaces[kBreakpointNs] = co_await queue.NextAwaiter();
}

Coroutine VimAsyncDriver::InitializeBuffers() {
  const Vector<int64_t> all_buffers = co_await PromiseBufferList();
  StringRpcQueue names_queue = PrepareStringQueue();
  for (size_t i = 0; i < all_buffers.Size(); ++i) {
    PromiseBufferName(all_buffers[i]).Enqueue(names_queue);
  }
  memset(buffers, -1, sizeof(buffers));

  StringSlice names[kTotalBufs];
  names[kCaptureBuf] = "Gdb capture";
  names[kAsmBuf] = "Gdb disas";
  names[kPromptBuf] = "Gdb prompt";
  names[kIoBuf] = "Gdb i/o";

  for (size_t i = 0; i < all_buffers.Size(); ++i) {
    FixedString dynamic_str = co_await names_queue.NextAwaiter();
    StringSlice name = dynamic_str.ToSlice();
    if (name.Size() >= 1) {
      switch (name[name.Size() - 1]) {
        case 'e':
          if (PDP_LIKELY(name.EndsWith(names[kCaptureBuf]))) {
            buffers[kCaptureBuf] = all_buffers[i];
          }
          break;
        case 's':
          if (PDP_LIKELY(name.EndsWith(names[kAsmBuf]))) {
            buffers[kAsmBuf] = all_buffers[i];
          }
          break;
        case 't':
          if (PDP_LIKELY(name.EndsWith(names[kPromptBuf]))) {
            buffers[kPromptBuf] = all_buffers[i];
          }
          break;
        case 'o':
          if (PDP_LIKELY(name.EndsWith(names[kIoBuf]))) {
            buffers[kIoBuf] = all_buffers[i];
          }
          break;
      }
    }
    opened_buffers.EmplaceUnchecked(std::move(dynamic_str), all_buffers[i]);
  }

  IntegerRpcQueue new_buffers_queue = PrepareIntegerQueue();
  for (size_t i = 0; i < kTotalBufs; ++i) {
    if (buffers[i] < 0) {
      PromiseCreateBuffer().Enqueue(new_buffers_queue);
    }
  }
  for (size_t i = 0; i < kTotalBufs; ++i) {
    if (buffers[i] < 0) {
      buffers[i] = co_await new_buffers_queue.NextAwaiter();
      vim_driver.SendRpcRequest("nvim_buf_set_name", buffers[i], names[i]);
    }
  }

  vim_driver.SendRpcRequest("nvim_buf_set_lines", buffers[kPromptBuf], 0, -1, false,
                            std::initializer_list<StringSlice>{});
  vim_driver.SendRpcRequest("nvim_buf_set_option", buffers[kPromptBuf], "modified", false);
  num_prompt_lines = 0;
}

}  // namespace pdp

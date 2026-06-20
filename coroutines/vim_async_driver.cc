#include "vim_async_driver.h"

namespace pdp {

IntegerRpcAwaiter IntegerRpcQueue::NextAwaiter() {
  return IntegerRpcAwaiter(async_driver, token_begin++);
}

StringRpcAwaiter StringRpcQueue::NextAwaiter() {
  return StringRpcAwaiter(async_driver, token_begin++);
}

VimAsyncDriver::VimAsyncDriver(int vim_input_fd, int vim_output_fd)
    : vim_driver(vim_input_fd, vim_output_fd), last_pc_bufnr(-1) {
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
      PDP_FMT_UNREACHABLE("Unexpected number of elements: {}", elems);
    }
    auto bufnr = vim_driver.ReadInteger();
    auto name = vim_driver.ReadString();
    if (name.ToSlice().StartsWith('/') && FileReadable(name.Cstr())) {
      auto [it, _] = opened_buffers.Emplace(std::move(name));
      it->value = bufnr;
      HandleNewBuffer(it->key.ToSlice(), it->value);
    }
  } else if (method == "pdp/buf_removed") {
    if (PDP_UNLIKELY(elems != 1)) {
      PDP_FMT_UNREACHABLE("Unexpected number of elements: {}", elems);
    }
    auto name = vim_driver.ReadString();
    auto it = opened_buffers.Find(name.ToSlice());
    if (it != opened_buffers.End()) {
      opened_buffers.Erase(it);
    }
  } else {
    PDP_FMT_UNREACHABLE("Unhandled notification {}", method.ToSlice());
  }
}

Coroutine VimAsyncDriver::HandleNewBuffer(const StringSlice &bufname, int bufnr) {
  pdp_info("Triggered notify event fullname={} bufnr={}", bufname, bufnr);
  auto it = deferred_br.Find(bufname);
  if (it != deferred_br.End()) {
    // Materialize bufname into a permanent string. This is required since we are going to suspend.
    auto [bufname, ext_stack] = deferred_br.Extract(it);

    IntegerRpcQueue queue = PrepareIntegerQueue();
    for (size_t i = 0; i < ext_stack.Size(); ++i) {
      PromiseBreakpointMark(ext_stack[i].sign_text, bufnr, ext_stack[i].lnum, ext_stack[i].enabled)
          .Enqueue(queue);
      placed_br.EmplaceUnchecked(FileLineView(bufname.ToSlice(), ext_stack[i].lnum), 0);
    }
    for (size_t i = 0; i < ext_stack.Size(); ++i) {
      auto extmark_id = co_await queue.NextAwaiter();
      auto it = placed_br.Find(FileLineView(bufname.ToSlice(), ext_stack[i].lnum));
      if (PDP_LIKELY(it != placed_br.End())) {
        it->value = extmark_id;
      } else {
        // Very unlikely - mark was deleted during creation.
        DeleteBreakpointMark(bufnr, extmark_id);
      }
    }
    pdp_assert(queue.Empty());
  }
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

void VimAsyncDriver::DeleteBreakpointMark(const StringSlice &fullname, int lnum) {
  auto extmark_it = placed_br.Find(FileLineView(fullname, lnum));
  if (extmark_it != placed_br.End()) {
    pdp_assert(deferred_br.Find(fullname) == deferred_br.End());
    auto buffer_it = opened_buffers.Find(fullname);
    if (PDP_LIKELY(buffer_it != opened_buffers.End())) {
      DeleteBreakpointMark(buffer_it->value, extmark_it->value);
    }
    placed_br.Erase(extmark_it);
  } else {
    auto defer_it = deferred_br.Find(fullname);
    if (PDP_LIKELY(defer_it != deferred_br.End())) {
      for (size_t i = 0; i < defer_it->value.Size(); ++i) {
        if (defer_it->value[i].lnum == lnum) {
          defer_it->value[i] = defer_it->value.Top();
          defer_it->value.Pop();
          break;
        }
      }
      if (defer_it->value.Empty()) {
        deferred_br.Erase(defer_it);
      }
    }
  }
}

void VimAsyncDriver::SetBreakpointMark(const StringSlice &mark, const StringSlice &fullname,
                                       int lnum, bool enabled) {
  char sign_text[3];
  memset(sign_text, 0, sizeof(sign_text));
  auto sign_length = mark.Size() <= 2 ? mark.Size() : 2;
  memcpy(sign_text, mark.Data(), sign_length);

  auto it = opened_buffers.Find(fullname);
  if (it != opened_buffers.End()) {
    placed_br.EmplaceUnchecked(FileLineView(fullname, lnum), 0);
    HandleBreakpointMark(fullname, it->value, lnum, sign_text, enabled);
  } else {
    auto [stack_it, _] = deferred_br.Emplace(fullname);
    stack_it->value.Emplace(sign_text, lnum, enabled);
  }
}

void VimAsyncDriver::ResetProgramCursor(int bufnr, int lnum) {
  if (PDP_LIKELY(last_pc_bufnr >= 0)) {
    vim_driver.SendRpcRequest("nvim_buf_clear_namespace", last_pc_bufnr,
                              namespaces[kProgramCounterNs], 0, -1);
  }
  last_pc_bufnr = bufnr;

  RpcBuilder builder;
  vim_driver.BeginRpcRequest(builder, "nvim_buf_set_extmark", bufnr, namespaces[kProgramCounterNs],
                             lnum - 1, 0);
  builder.OpenShortMap();
  builder.AddMapItem("line_hl_group", "debugPC");
  builder.CloseShortMap();
  vim_driver.EndRpcRequest(builder);
}

IntegerRpcAwaiter VimAsyncDriver::PromiseBreakpointMark(char mark[3], int bufnr, int lnum,
                                                        bool enabled) {
  RpcBuilder builder;
  auto token = vim_driver.BeginRpcRequest(builder, "nvim_buf_set_extmark", bufnr,
                                          namespaces[kBreakpointNs], lnum - 1, 0);
  builder.OpenShortMap();
  builder.AddMapItem("sign_text", StringSlice(mark));
  builder.AddMapItem("sign_hl_group", enabled ? "debugBreakpoint" : "debugBreakpointDisabled");
  builder.CloseShortMap();
  vim_driver.EndRpcRequest(builder);
  return IntegerRpcAwaiter(this, token);
}

void VimAsyncDriver::DeleteBreakpointMark(int bufnr, int extmark) {
  vim_driver.SendRpcRequest("nvim_buf_del_extmark", bufnr, namespaces[kBreakpointNs], extmark);
}

Coroutine VimAsyncDriver::HandleBreakpointMark(NoSuspendRef<const StringSlice> fullname, int bufnr,
                                               int lnum, char mark[3], bool enabled) {
  Hash<FileLineView> hasher;
  auto hash = hasher(FileLineView(fullname.Get(), lnum));
  fullname.Release();

  auto extmark = co_await PromiseBreakpointMark(mark, bufnr, lnum, enabled);

  struct Equals {
    Equals(int b, int l) : bufnr(b), lnum(l) {}

    bool operator()(const emhash8::FileLineMap<int>::Entry &e) const {
      return e.key.lnum == lnum && e.value == bufnr;
    }

   private:
    int bufnr;
    int lnum;
  } eq(bufnr, lnum);

  auto it = placed_br.FindByHash(hash, eq);
  if (PDP_LIKELY(it != placed_br.End())) {
    it->value = extmark;
  } else {
    // Very unlikely - mark was deleted during creation.
    DeleteBreakpointMark(bufnr, extmark);
  }
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
      // Vim quirk: There can be multiple buffers with no. Do not use 'EmplaceUnchecked' here.
      opened_buffers.Emplace(std::move(dynamic_str), all_buffers[i]);
    }
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

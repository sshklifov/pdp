#pragma once

#include "coroutine.h"
#include "drivers/vim_driver.h"
#include "external/emhash8.h"
#include "system/poll_table.h"

namespace pdp {

struct MessageBuilder : public NonCopyableNonMovable {
  struct MessagePart {
    size_t length;
    StringSlice highlight;
  };

  MessageBuilder() : num_parts(0) {}

  void Append(const StringSlice &msg, const StringSlice &hl) {
    if (PDP_UNLIKELY(num_parts >= max_parts)) {
      PDP_UNREACHABLE("MessageBuilder: overflow");
    }
    joined_message.MemCopy(msg);
    parts[num_parts].length = msg.Size();
    parts[num_parts].highlight = hl;
    ++num_parts;
  }

  template <typename... Args>
  void AppendFormat(const StringSlice &hl, const StringSlice &fmt, Args &&...args) {
    if (PDP_UNLIKELY(num_parts >= max_parts)) {
      PDP_UNREACHABLE("MessageBuilder: overflow");
    }

    auto packed_args = MakePackedArgs(std::forward<Args>(args)...);
    auto length = joined_message.AppendPack(fmt, packed_args.slots, packed_args.type_bits);

    parts[num_parts].length = length;
    parts[num_parts].highlight = hl;
    ++num_parts;
  }

  size_t GetJoinedMessageLength() const { return joined_message.Size(); }

  StringSlice GetJoinedMessage() const {
    return StringSlice(joined_message.Begin(), joined_message.End());
  }

  const MessagePart *begin() const { return parts; }

  const MessagePart *end() const { return parts + num_parts; }

  void Reset() {
    joined_message.Clear();
    num_parts = 0;
  }

 private:
  size_t num_parts;

  static constexpr size_t max_parts = 16;

  StringVector joined_message;
  MessagePart parts[max_parts];
};

struct BooleanRpcAwaiter;
struct StringRpcAwaiter;
struct IntegerRpcAwaiter;
struct IntegerArrayRpcAwaiter;

struct IntegerRpcQueue;
struct StringRpcQueue;

struct VimAsyncDriver {
  friend struct BooleanRpcAwaiter;
  friend struct StringRpcAwaiter;
  friend struct IntegerRpcAwaiter;
  friend struct IntegerArrayRpcAwaiter;

  friend struct IntegerRpcQueue;
  friend struct StringRpcQueue;

  VimAsyncDriver(int vim_input_fd, int vim_output_fd);

  void RegisterForPoll(PollTable &table);
  void OnPollResults(PollTable &table);

  IntegerRpcQueue PrepareIntegerQueue();
  StringRpcQueue PrepareStringQueue();

  IntegerRpcAwaiter PromiseCreateBuffer();
  IntegerRpcAwaiter PromiseNamespace(const StringSlice &ns);
  StringRpcAwaiter PromiseBufferName(int64_t buffer);
  IntegerArrayRpcAwaiter PromiseBufferList();

  IntegerRpcAwaiter PromiseBufferLineCount(int bufnr);

  void DeleteBreakpointMark(const StringSlice &fullname, int extmark);
  void SetBreakpointMark(const StringSlice &mark, const StringSlice &fullname, int lnum,
                         int enabled);

  void ShowNormal(const StringSlice &msg);

  template <typename... Args>
  void ShowNormal(const StringSlice &fmt, Args &&...args) {
    auto packed_args = MakePackedArgs(std::forward<Args>(args)...);
    ShowPacked(fmt, packed_args.slots, packed_args.type_bits);
  }

  template <typename... Args>
  void ShowWarning(const StringSlice &fmt, Args &&...args) {
    auto packed_args = MakePackedArgs(std::forward<Args>(args)...);
    ShowPacked(fmt, packed_args.slots, packed_args.type_bits);
    HighlightLastLine("WarningMsg");
  }

  void ShowMessage(const MessageBuilder &builder);

  void HighlightLastLine(const StringSlice &hl);
  void HighlightLastLine(int start_col, int end_col, const StringSlice &hl);

 private:
  Coroutine InitializeNs();
  Coroutine InitializeBuffers();

  void Drain();
  void ReadNotifyEvent();
  void OnNotifyNewBuffer(const StringSlice &fullname, int bufnr);

  void SetBreakpointMark(const StringSlice &mark, int bufnr, int lnum, int enabled);
  void ShowPacked(const StringSlice &fmt, PackedValue *args, uint64_t type_bits);

  VimDriver vim_driver;
  CoroutineTokenTable suspended_handlers;
  emhash8::StringMap<int64_t> opened_buffers;
  emhash8::StringMap<FixedString> pending_extmarks;  // TODO change template arg

  unsigned num_prompt_lines;

  enum VimNamespaces { kProgramCounterNs, kPromptBufferNs, kBreakpointNs, kTotalNs };

  int namespaces[kTotalNs];

  enum VimBuffers { kCaptureBuf, kAsmBuf, kPromptBuf, kIoBuf, kTotalBufs };

  int buffers[kTotalBufs];
  int cursor_buffer;
};

struct IntegerRpcQueue {
  VimAsyncDriver *async_driver;
  uint32_t token_begin;
  uint32_t token_end;

  IntegerRpcQueue(VimAsyncDriver *d, uint32_t token)
      : async_driver(d), token_begin(token), token_end(token) {}

  ~IntegerRpcQueue() { pdp_assert(Empty()); }

  size_t Size() const { return token_end - token_begin; }

  bool Empty() const { return token_begin == token_end; }

  IntegerRpcAwaiter NextAwaiter();
};

struct StringRpcQueue {
  VimAsyncDriver *async_driver;
  uint32_t token_begin;
  uint32_t token_end;

  StringRpcQueue(VimAsyncDriver *d, uint32_t token)
      : async_driver(d), token_begin(token), token_end(token) {}

  ~StringRpcQueue() { pdp_assert(Empty()); }

  size_t Size() const { return token_end - token_begin; }

  bool Empty() const { return token_begin == token_end; }

  StringRpcAwaiter NextAwaiter();
};

struct BooleanRpcAwaiter : public NonCopyableNonMovable {
  VimAsyncDriver *async_driver;
  uint32_t token;

  BooleanRpcAwaiter() : async_driver(nullptr), token(0) {}
  BooleanRpcAwaiter(VimAsyncDriver *c, uint32_t t) : async_driver(c), token(t) {}

  bool await_ready() const noexcept { return false; }

  void await_suspend(std::coroutine_handle<Coroutine::promise_type> coro) const noexcept {
    async_driver->suspended_handlers.Suspend(token, coro);
  }

  bool await_resume() noexcept { return async_driver->vim_driver.ReadBool(); }
};

struct StringRpcAwaiter {
  VimAsyncDriver *async_driver;
  uint32_t token;

  StringRpcAwaiter() : async_driver(nullptr), token(0) {}
  StringRpcAwaiter(VimAsyncDriver *c, uint32_t t) : async_driver(c), token(t) {}

  void Enqueue(StringRpcQueue &queue) && {
    pdp_assert(queue.token_end == token);
    queue.token_end++;
#ifdef PDP_ENABLE_ASSERT
    async_driver = nullptr;
#endif
  }

  bool await_ready() const noexcept {
    pdp_assert(async_driver);
    return false;
  }

  void await_suspend(std::coroutine_handle<Coroutine::promise_type> coro) const noexcept {
    pdp_assert(async_driver);
    async_driver->suspended_handlers.Suspend(token, coro);
  }

  FixedString await_resume() noexcept {
    pdp_assert(async_driver);
    return async_driver->vim_driver.ReadString();
  }
};

struct IntegerRpcAwaiter {
  VimAsyncDriver *async_driver;
  uint32_t token;

  IntegerRpcAwaiter() : async_driver(nullptr), token(0) {}
  IntegerRpcAwaiter(VimAsyncDriver *c, uint32_t t) : async_driver(c), token(t) {}

  void Enqueue(IntegerRpcQueue &queue) && {
    pdp_assert(queue.token_end == token);
    queue.token_end++;
#ifdef PDP_ENABLE_ASSERT
    async_driver = nullptr;
#endif
  }

  bool await_ready() const noexcept {
    pdp_assert(async_driver);
    return false;
  }

  void await_suspend(std::coroutine_handle<Coroutine::promise_type> coro) const noexcept {
    pdp_assert(async_driver);
    async_driver->suspended_handlers.Suspend(token, coro);
  }

  int64_t await_resume() noexcept {
    pdp_assert(async_driver);
    return async_driver->vim_driver.ReadInteger();
  }
};

struct IntegerArrayRpcAwaiter {
  VimAsyncDriver *async_driver;
  uint32_t token;

  IntegerArrayRpcAwaiter() : async_driver(nullptr), token(0) {}
  IntegerArrayRpcAwaiter(VimAsyncDriver *c, uint32_t t) : async_driver(c), token(t) {}

  bool await_ready() const noexcept { return false; }

  void await_suspend(std::coroutine_handle<Coroutine::promise_type> coro) const noexcept {
    async_driver->suspended_handlers.Suspend(token, coro);
  }

  Vector<int64_t> await_resume() noexcept {
    uint32_t length = async_driver->vim_driver.OpenArray();
    Vector<int64_t> list(length);
    for (uint32_t i = 0; i < length; ++i) {
      list += async_driver->vim_driver.ReadInteger();
    }
    return list;
  }
};

}  // namespace pdp

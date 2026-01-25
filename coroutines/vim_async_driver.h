#pragma once

#include "drivers/vim_driver.h"
#include "handler_coroutine.h"
#include "system/poll_table.h"

namespace pdp {

struct BooleanRpcAwaiter;
struct StringRpcAwaiter;
struct IntegerRpcAwaiter;
struct IntegerArrayRpcAwaiter;

struct VimAsyncDriver {
  friend struct BooleanRpcAwaiter;
  friend struct StringRpcAwaiter;
  friend struct IntegerRpcAwaiter;
  friend struct IntegerArrayRpcAwaiter;

  VimAsyncDriver(int vim_input_fd, int vim_output_fd);

  void RegisterForPoll(PollTable &table);
  void OnPollResults(PollTable &table);

  IntegerArrayRpcAwaiter PromiseBufferList();

  void ShowNormal(const StringSlice &msg);
  template <typename... Args>
  void ShowNormal(const StringSlice &fmt, Args &&...args) {
    auto packed_args = MakePackedArgs(std::forward<Args>(args)...);
    ShowPacked(fmt, packed_args.slots, packed_args.type_bits);
  }

  void ShowWarning(const StringSlice &msg);
  void ShowError(const StringSlice &msg);
  void ShowMessage(const StringSlice &msg, const StringSlice &hl);
  void ShowMessage(std::initializer_list<StringSlice> m, std::initializer_list<StringSlice> h);

 private:
  HandlerCoroutine InitializeNs();
  HandlerCoroutine InitializeBuffers();

  void Drain();
  void ShowPacked(const StringSlice &fmt, PackedValue *args, uint64_t type_bits);

  VimDriver vim_driver;
  HandlerTable suspended_handlers;

  unsigned num_prompt_lines;

  enum VimNamespaces {
    kHighlightNs,
    kProgramCounterNs,
    kRegisterNs,
    kPromptBufferNs,
    kConcealVarNs,
    kConcealJumpNs,
    kBreakpointNs,
    kTotalNs
  };

  int namespaces[kTotalNs];

  enum VimBuffers { kCaptureBuf, kAsmBuf, kPromptBuf, kIoBuf, kTotalBufs };

  int buffers[kTotalBufs];
};

struct BooleanRpcAwaiter : public NonCopyableNonMovable {
  VimAsyncDriver *async_driver;
  uint32_t token;

  BooleanRpcAwaiter(VimAsyncDriver *c, uint32_t t) : async_driver(c), token(t) {}

  bool await_ready() const noexcept { return false; }

  void await_suspend(std::coroutine_handle<HandlerCoroutine::promise_type> coro) const noexcept {
    async_driver->suspended_handlers.Suspend(token, coro);
  }

  bool await_resume() noexcept { return async_driver->vim_driver.ReadBoolResult(); }
};

struct StringRpcAwaiter {
  VimAsyncDriver *async_driver;
  uint32_t token;

  StringRpcAwaiter(VimAsyncDriver *c, uint32_t t) : async_driver(c), token(t) {}

  bool await_ready() const noexcept { return false; }

  void await_suspend(std::coroutine_handle<HandlerCoroutine::promise_type> coro) const noexcept {
    async_driver->suspended_handlers.Suspend(token, coro);
  }

  DynamicString await_resume() noexcept { return async_driver->vim_driver.ReadStringResult(); }
};

struct IntegerRpcAwaiter {
  VimAsyncDriver *async_driver;
  uint32_t token;

  IntegerRpcAwaiter(VimAsyncDriver *c, uint32_t t) : async_driver(c), token(t) {}

  bool await_ready() const noexcept { return false; }

  void await_suspend(std::coroutine_handle<HandlerCoroutine::promise_type> coro) const noexcept {
    async_driver->suspended_handlers.Suspend(token, coro);
  }

  int64_t await_resume() noexcept { return async_driver->vim_driver.ReadIntegerResult(); }
};

struct IntegerArrayRpcAwaiter {
  VimAsyncDriver *async_driver;
  uint32_t token;

  IntegerArrayRpcAwaiter(VimAsyncDriver *c, uint32_t t) : async_driver(c), token(t) {}

  bool await_ready() const noexcept { return false; }

  void await_suspend(std::coroutine_handle<HandlerCoroutine::promise_type> coro) const noexcept {
    async_driver->suspended_handlers.Suspend(token, coro);
  }

  Vector<int64_t> await_resume() noexcept {
    uint32_t length = async_driver->vim_driver.OpenArrayResult();
    Vector<int64_t> list(length);
    for (uint32_t i = 0; i < length; ++i) {
      list += async_driver->vim_driver.ReadIntegerResult();
    }
    return list;
  }
};

}  // namespace pdp

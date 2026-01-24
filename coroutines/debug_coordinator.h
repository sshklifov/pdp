#pragma once

#include "coroutines/gdb_async_driver.h"
#include "data/vector.h"
#include "drivers/ssh_driver.h"
#include "drivers/vim_driver.h"
#include "strings/dynamic_string.h"

#include "debug_session.h"
#include "handler_coroutine.h"
#include "system/poll_table.h"

namespace pdp {

struct BooleanRpcAwaiter;
struct StringRpcAwaiter;
struct IntegerRpcAwaiter;
struct IntegerArrayRpcAwaiter;

struct DebugCoordinator {
  friend struct BooleanRpcAwaiter;
  friend struct StringRpcAwaiter;
  friend struct IntegerRpcAwaiter;
  friend struct IntegerArrayRpcAwaiter;

  DebugCoordinator(const StringSlice &host, int vim_input_fd, int vim_output_fd,
                   ChildReaper &reaper);

  ~DebugCoordinator();

  void RegisterForPoll(PollTable &table);
  void OnPollResults(PollTable &table);

  bool IsIdle() const;
  void PrintActivity() const;

  // TODO
  bool IsRemoteDebugging() const { return false; }
  StringSlice GetHost() const { return ""; }

  DebugSession &Session() { return session_data; }
  int Namespace(VimNamespaces ns) const { return session_data.namespaces[ns]; }
  int Buffer(VimBuffers buf) const { return session_data.buffers[buf]; }
  int GetExeTimetamp();

  BooleanRpcAwaiter RpcBufExists(int bufnr);
  IntegerArrayRpcAwaiter RpcListBuffers();
  void RpcClearNamespace(int bufnr, int ns);

  void RpcShowNormal(const StringSlice &msg);
  template <typename... Args>
  void RpcShowNormal(const StringSlice &fmt, Args &&...args) {
    auto packed_args = MakePackedArgs(std::forward<Args>(args)...);
    RpcShowPacked(fmt, packed_args.slots, packed_args.type_bits);
  }

  void RpcShowWarning(const StringSlice &msg);
  void RpcShowError(const StringSlice &msg);
  void RpcShowMessage(const StringSlice &msg, const StringSlice &hl);
  void RpcShowMessage(std::initializer_list<StringSlice> m, std::initializer_list<StringSlice> h);

 private:
  void DrainVim();

  void RpcShowPacked(const StringSlice &fmt, PackedValue *args, uint64_t type_bits);

  HandlerCoroutine InitializeNs();
  HandlerCoroutine InitializeBuffers();

  DefaultAllocator allocator;
  SshDriver *ssh_driver;

  GdbAsyncDriver gdb_async;
  VimDriver vim_driver;
  DebugSession session_data;

  HandlerTable suspended_handlers;
};

// TODO check token valid?! idk
struct BooleanRpcAwaiter : public NonCopyableNonMovable {
  DebugCoordinator *coordinator;
  uint32_t token;

  BooleanRpcAwaiter(DebugCoordinator *c, uint32_t t) : coordinator(c), token(t) {}

  bool await_ready() const noexcept { return false; }

  void await_suspend(std::coroutine_handle<HandlerCoroutine::promise_type> coro) const noexcept {
    coordinator->suspended_handlers.Suspend(token, coro);
  }

  bool await_resume() noexcept { return coordinator->vim_driver.ReadBoolResult(); }
};

struct StringRpcAwaiter {
  DebugCoordinator *coordinator;
  uint32_t token;

  StringRpcAwaiter(DebugCoordinator *c, uint32_t t) : coordinator(c), token(t) {}

  bool await_ready() const noexcept { return false; }

  void await_suspend(std::coroutine_handle<HandlerCoroutine::promise_type> coro) const noexcept {
    coordinator->suspended_handlers.Suspend(token, coro);
  }

  DynamicString await_resume() noexcept { return coordinator->vim_driver.ReadStringResult(); }
};

struct IntegerRpcAwaiter {
  DebugCoordinator *coordinator;
  uint32_t token;

  IntegerRpcAwaiter(DebugCoordinator *c, uint32_t t) : coordinator(c), token(t) {}

  bool await_ready() const noexcept { return false; }

  void await_suspend(std::coroutine_handle<HandlerCoroutine::promise_type> coro) const noexcept {
    coordinator->suspended_handlers.Suspend(token, coro);
  }

  int64_t await_resume() noexcept { return coordinator->vim_driver.ReadIntegerResult(); }
};

struct IntegerArrayRpcAwaiter {
  DebugCoordinator *coordinator;
  uint32_t token;

  IntegerArrayRpcAwaiter(DebugCoordinator *c, uint32_t t) : coordinator(c), token(t) {}

  bool await_ready() const noexcept { return false; }

  void await_suspend(std::coroutine_handle<HandlerCoroutine::promise_type> coro) const noexcept {
    coordinator->suspended_handlers.Suspend(token, coro);
  }

  Vector<int64_t> await_resume() noexcept {
    uint32_t length = coordinator->vim_driver.OpenArrayResult();
    Vector<int64_t> list(length);
    for (uint32_t i = 0; i < length; ++i) {
      list += coordinator->vim_driver.ReadIntegerResult();
    }
    return list;
  }
};

}  // namespace pdp

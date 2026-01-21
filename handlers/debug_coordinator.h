#pragma once

#include "data/scoped_ptr.h"
#include "data/vector.h"
#include "drivers/gdb_driver.h"
#include "drivers/vim_driver.h"
#include "parser/expr.h"
#include "strings/dynamic_string.h"

#include "handler_coroutine.h"
#include "debug_session.h"

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

  DebugCoordinator(int vim_input_fd, int vim_output_fd);

  void PollGdb(Milliseconds timeout);
  void PollVim(Milliseconds timeout);
  void ReachIdle(Milliseconds timeout);

  // awaitable = SshCommand()
  // awaitable = <this, fd>
  // await_resume -> return DynamicString
  // TODO

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
  void RpcShowPacked(const StringSlice &fmt, PackedValue *args, uint64_t type_bits);
  void HandleAsync(GdbAsyncKind kind, ScopedPtr<ExprBase> &&expr);
  void HandleResult(GdbResultKind kind, ScopedPtr<ExprBase> &&expr);

  HandlerCoroutine InitializeNs();
  HandlerCoroutine InitializeBuffers();

  GdbDriver gdb_driver;
  VimDriver vim_controller;
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

  bool await_resume() noexcept { return coordinator->vim_controller.ReadBoolResult(); }
};

struct StringRpcAwaiter {
  DebugCoordinator *coordinator;
  uint32_t token;

  StringRpcAwaiter(DebugCoordinator *c, uint32_t t) : coordinator(c), token(t) {}

  bool await_ready() const noexcept { return false; }

  void await_suspend(std::coroutine_handle<HandlerCoroutine::promise_type> coro) const noexcept {
    coordinator->suspended_handlers.Suspend(token, coro);
  }

  DynamicString await_resume() noexcept { return coordinator->vim_controller.ReadStringResult(); }
};

struct IntegerRpcAwaiter {
  DebugCoordinator *coordinator;
  uint32_t token;

  IntegerRpcAwaiter(DebugCoordinator *c, uint32_t t) : coordinator(c), token(t) {}

  bool await_ready() const noexcept { return false; }

  void await_suspend(std::coroutine_handle<HandlerCoroutine::promise_type> coro) const noexcept {
    coordinator->suspended_handlers.Suspend(token, coro);
  }

  int64_t await_resume() noexcept { return coordinator->vim_controller.ReadIntegerResult(); }
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
    uint32_t length = coordinator->vim_controller.OpenArrayResult();
    Vector<int64_t> list(length);
    for (uint32_t i = 0; i < length; ++i) {
      list += coordinator->vim_controller.ReadIntegerResult();
    }
    return list;
  }
};

}  // namespace pdp

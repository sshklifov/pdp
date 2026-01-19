#pragma once

#include "data/scoped_ptr.h"
#include "data/vector.h"
#include "drivers/gdb_driver.h"
#include "drivers/vim_driver.h"
#include "parser/expr.h"
#include "strings/dynamic_string.h"

#include "coroutine_table.h"
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

  DebugSession &GetSessionData() { return session_data; }
  const DebugSession &GetSessionData() const { return session_data; }

  BooleanRpcAwaiter BufExists(int64_t bufnr);
  IntegerArrayRpcAwaiter ListBuffers();

  void ShowNormal(const StringSlice &msg);
  void ShowWarning(const StringSlice &msg);
  void ShowError(const StringSlice &msg);
  void ShowMessage(const StringSlice &msg, const StringSlice &hl);
  void ShowMessage(std::initializer_list<StringSlice> msg, std::initializer_list<StringSlice> hl);

 private:
  void HandleAsync(GdbAsyncKind kind, ScopedPtr<ExprBase> &&expr);
  void HandleResult(GdbResultKind kind, ScopedPtr<ExprBase> &&expr);

  HandlerCoroutine InitializeNs();
  HandlerCoroutine InitializeBuffers();

  GdbDriver gdb_driver;
  VimDriver vim_controller;
  DebugSession session_data;

  CoroutineTable suspended_handlers;
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

#pragma once

#include "application/debug_session.h"
#include "application/gdb_driver.h"
#include "application/vim_controller.h"
#include "data/scoped_ptr.h"
#include "parser/expr.h"

#include "coroutine_table.h"

namespace pdp {

struct BooleanRpcAwaiter;

struct DebugCoordinator {
  friend struct BooleanRpcAwaiter;

  DebugCoordinator(int vim_input_fd, int vim_output_fd);

  void PollGdb(Milliseconds timeout);
  void PollVim(Milliseconds timeout);

  DebugSession &GetSessionData() { return session_data; }
  const DebugSession &GetSessionData() const { return session_data; }

  BooleanRpcAwaiter BufExists(int bufnr);

 private:
  void HandleAsync(GdbAsyncKind kind, ScopedPtr<ExprBase> &&expr);
  void HandleResult(GdbResultKind kind, ScopedPtr<ExprBase> &&expr);

  GdbDriver gdb_driver;
  VimController vim_controller;
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

}  // namespace pdp

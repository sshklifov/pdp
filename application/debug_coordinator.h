#pragma once

#include "data/scoped_ptr.h"
#include "gdb_driver.h"
#include "parser/expr.h"
#include "vim_controller.h"

namespace pdp {

struct DebugCoordinator {
  void Poll(Milliseconds timeout);

 private:
  void HandleAsync(AsyncKind kind, ScopedPtr<ExprBase> &&expr);
  void HandleResult(ResultKind kind, ScopedPtr<ExprBase> &&expr);

  GdbDriver gdb_driver;
  VimController vim_controller;

  CallbackTable<DebugCoordinator *, ScopedPtr<ExprBase>> waiting_handlers;
};

}  // namespace pdp

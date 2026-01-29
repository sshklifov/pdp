#pragma once

#include "coroutines/handlers.h"

namespace pdp {

void HandleThreadSelect(DebugCoordinator *d, UniquePtr<ExprBase> expr) {
  GdbExprView dict(expr);

  d->SetThreadSelected(dict["new-thread-id"].RequireInt());
  d->SetFrameSelected(dict["frame"]["level"].RequireInt());

  // RefreshCursorSign(d, std::move(expr));
}

}  // namespace pdp

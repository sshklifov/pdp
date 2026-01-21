#pragma once

#include "debug_coordinator.h"

namespace pdp {

inline HandlerCoroutine ClearCursorSign(DebugCoordinator *d) {
  int bufnr = d->Session().source_bufnr;
  const bool has_source = co_await d->RpcBufExists(bufnr);
  if (has_source) {
    int ns = d->Namespace(kProgramCounterNs);
    d->RpcClearNamespace(bufnr, ns);
  }
}

inline void PlaceAsmCursor(DebugCoordinator *d, ScopedPtr<ExprBase> &&expr) {
  // TODO
}

inline void PlaceSourceCursor(DebugCoordinator *d, ScopedPtr<ExprBase> &&expr) {
  // int ns = d->GetSessionData().namespaces[kProgramCounterNs];
  // TODO get()
  // auto filename = expr[""]
  LooseTypedView dict(expr);
  auto filename = dict["fullname"];
  auto line = dict["line"];
  if (!filename || !line) {
    d->RpcShowNormal("???\tNo source available.");
    return;
  }

  // TODO check timestamp. LOL this is difficult...
}

inline void PlaceCursorSign(DebugCoordinator *d, ScopedPtr<ExprBase> &&expr) {
  if (d->Session().asm_mode) {
    PlaceAsmCursor(d, std::move(expr));
  } else {
    PlaceSourceCursor(d, std::move(expr));
  }
}

inline void RefreshCursorSign(DebugCoordinator *d, ScopedPtr<ExprBase> &&expr) {
  ClearCursorSign(d);
  PlaceCursorSign(d, std::move(expr));
}

inline void HandleThreadSelect(DebugCoordinator *d, ScopedPtr<ExprBase> &&expr) {
  LooseTypedView dict(expr);

  d->Session().selected_thread = dict["new-thread-id"].AsInteger();
  d->Session().selected_frame = dict["frame"]["level"].AsInteger();

  RefreshCursorSign(d, std::move(expr));
}

inline void HandleProgramRun(DebugCoordinator *d, ScopedPtr<ExprBase> &&expr) {
  LooseTypedView dict(expr);
  int64_t pid = dict["pid"].AsInteger();
  if (d->IsRemoteDebugging()) {
    d->RpcShowNormal("Remote debugging {}", d->GetHost());
  } else {
    d->RpcShowNormal("Local debugging");
  }
  d->RpcShowNormal("Process id: {}", pid);

  // TODO 
  return;
}

}  // namespace pdp

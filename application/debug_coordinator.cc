#pragma once

#include "debug_coordinator.h"

#include "handlers/stream_handler.h"
#include "parser/mi_parser.h"

namespace pdp {

void DebugCoordinator::Poll(Milliseconds timeout) {
  GdbRecord record;
  RecordKind kind = gdb_driver.Poll(timeout, &record);
  if (PDP_UNLIKELY(kind != RecordKind::kNone)) {
    if (kind == RecordKind::kStream) {
      HandleStream h;
      return h(this, record.stream.message);
    } else {
      MiFirstPass first_pass(record.result_or_async.results);
      if (PDP_UNLIKELY(!first_pass.Parse())) {
        pdp_error("Pass #1 failed on: {}", record.result_or_async.results);
        return;
      }
      MiSecondPass second_pass(record.result_or_async.results, first_pass);
      ScopedPtr<ExprBase> expr(second_pass.Parse());
      if (PDP_UNLIKELY(!expr)) {
        pdp_error("Pass #2 failed on: {}", record.result_or_async.results);
        return;
      }
      if (kind == RecordKind::kAsync) {
        HandleAsync(static_cast<AsyncKind>(record.result_or_async.kind), std::move(expr));
      } else if (kind == RecordKind::kResult) {
        HandleResult(static_cast<ResultKind>(record.result_or_async.kind), std::move(expr));
      } else {
        pdp_assert(false);
      }
    }
  }
}

void DebugCoordinator::HandleResult(ResultKind kind, ScopedPtr<ExprBase> &&expr) {
  if (PDP_LIKELY(kind == ResultKind::kDone)) {
    // TODO
  } else if (PDP_LIKELY(kind == ResultKind::kError)) {
    // TODO
  }
}

void DebugCoordinator::HandleAsync(AsyncKind kind, ScopedPtr<ExprBase> &&expr) {
}

}  // namespace pdp

#include "debug_coordinator.h"

#include "handlers/handle_stream.h"

#include "parser/mi_parser.h"

namespace pdp {

DebugCoordinator::DebugCoordinator(int vim_input_fd, int vim_output_fd)
    : vim_controller(vim_input_fd, vim_output_fd) {
  gdb_driver.Start();
}

void DebugCoordinator::PollGdb(Milliseconds timeout) {
  GdbRecord record;
  RecordKind kind = gdb_driver.Poll(timeout, &record);
  if (PDP_UNLIKELY(kind != RecordKind::kNone)) {
    if (kind == RecordKind::kStream) {
      HandleStream(this, record.stream.message);
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
        HandleAsync(static_cast<GdbAsyncKind>(record.result_or_async.kind), std::move(expr));
      } else if (kind == RecordKind::kResult) {
        HandleResult(static_cast<GdbResultKind>(record.result_or_async.kind), std::move(expr));
      } else {
        pdp_assert(false);
      }
    }
  }
}

void DebugCoordinator::HandleResult(GdbResultKind kind, ScopedPtr<ExprBase> &&expr) {
  if (PDP_LIKELY(kind == GdbResultKind::kDone)) {
    // TODO
  } else if (PDP_LIKELY(kind == GdbResultKind::kError)) {
    // TODO
  }
}

void DebugCoordinator::HandleAsync(GdbAsyncKind kind, ScopedPtr<ExprBase> &&expr) {
  switch (kind) {
    case GdbAsyncKind::kThreadSelected: {
      // HandleThreadSelected h;
      // return h(this, std::move(expr));
    }
  }
}

void DebugCoordinator::PollVim(Milliseconds timeout) {
  uint32_t token = vim_controller.PollResponseToken(timeout);
  if (token != vim_controller.kInvalidToken) {
    suspended_handlers.Resume(token);
  }
}

BooleanRpcAwaiter DebugCoordinator::BufExists(int bufnr) {
  uint32_t token = vim_controller.SendRpcRequest("nvim_buf_is_valid", bufnr);
  return BooleanRpcAwaiter(this, token);
}

}  // namespace pdp

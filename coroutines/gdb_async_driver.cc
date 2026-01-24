#include "gdb_async_driver.h"
#include "parser/mi_parser.h"

namespace pdp {

GdbAsyncDriver::GdbAsyncDriver(ChildReaper &reaper) { gdb_driver.Start(reaper); }

void GdbAsyncDriver::RegisterForPoll(PollTable &table) {
  table.Register(gdb_driver.GetDescriptor());
  table.Register(gdb_driver.GetErrorDescriptor());
}

void GdbAsyncDriver::OnPollResults(PollTable &table) {
  if (table.HasInputEventsUnchecked(gdb_driver.GetDescriptor())) {
    DrainRecords();
  } else if (table.HasInputEventsUnchecked(gdb_driver.GetErrorDescriptor())) {
    DrainErrors();
  }
}

void GdbAsyncDriver::DrainRecords() {
  GdbRecord record;
  GdbRecordKind kind = gdb_driver.PollForRecords(&record);
  while (kind != GdbRecordKind::kNone) {
    if (kind == GdbRecordKind::kStream) {
      HandleStream(record.stream.message);
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
      if (kind == GdbRecordKind::kAsync) {
        HandleAsync(static_cast<GdbAsyncKind>(record.result_or_async.kind), std::move(expr));
      } else if (kind == GdbRecordKind::kResult) {
        HandleResult(static_cast<GdbResultKind>(record.result_or_async.kind), std::move(expr));
      } else {
        pdp_assert(false);
      }
    }
    kind = gdb_driver.PollForRecords(&record);
  }
}

void GdbAsyncDriver::DrainErrors() {
  StringSlice error = gdb_driver.PollForErrors();
  while (!error.Empty()) {
    pdp_error("Gdb error");
    pdp_error_multiline(error);
    error = gdb_driver.PollForErrors();
  }
}

void GdbAsyncDriver::HandleStream(const StringSlice &msg) {
  // TODO
  PDP_IGNORE(msg);
}

void GdbAsyncDriver::HandleAsync(GdbAsyncKind kind, ScopedPtr<ExprBase> expr) {
  // TODO
  PDP_IGNORE(kind);
  PDP_IGNORE(expr);
}

void GdbAsyncDriver::HandleResult(GdbResultKind kind, ScopedPtr<ExprBase> expr) {
  // TODO
  PDP_IGNORE(kind);
  PDP_IGNORE(expr);
}

}  // namespace pdp

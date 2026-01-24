#pragma once

#include "drivers/gdb_driver.h"
#include "parser/expr.h"
#include "system/child_reaper.h"
#include "system/poll_table.h"

namespace pdp {

struct GdbAsyncDriver {
  GdbAsyncDriver(ChildReaper &reaper);

  void RegisterForPoll(PollTable &table);
  void OnPollResults(PollTable &table);

 private:
  void DrainRecords();
  void DrainErrors();

  void HandleStream(const StringSlice &msg);
  void HandleAsync(GdbAsyncKind kind, ScopedPtr<ExprBase> expr);
  void HandleResult(GdbResultKind kind, ScopedPtr<ExprBase> expr);

  GdbDriver gdb_driver;
};

}  // namespace pdp

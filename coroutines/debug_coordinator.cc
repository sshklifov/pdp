#include "debug_coordinator.h"

#include "handlers.h"  // TODO?

namespace pdp {

DebugCoordinator::DebugCoordinator(const StringSlice &host, int vim_input_fd, int vim_output_fd,
                                   ChildReaper &reaper)
    : ssh_driver(nullptr), gdb_async(reaper), vim_async(vim_input_fd, vim_output_fd) {
  inferior_pid = -1;
  thread_selected = 1;
  frame_selected = 0;

  if (!host.Empty()) {
    ssh_driver = Allocate<SshDriver>(allocator, 1);
    new (ssh_driver) SshDriver(host, reaper);
  }

  gdb_async.SetStreamCallback([this](StringSlice msg) { OnGdbStream(msg); });
  gdb_async.SetAsyncCallback(
      [this](GdbAsyncKind kind, UniquePtr<ExprBase> expr) { OnGdbAsync(kind, std::move(expr)); });
  gdb_async.SetResultCallback(
      [this](GdbResultKind kind, UniquePtr<ExprBase> expr) { OnGdbResult(kind, std::move(expr)); });
  gdb_async.SetErrorCallback([this](StringSlice msg) { OnGdbError(msg); });
}

DebugCoordinator::~DebugCoordinator() {
  if (ssh_driver) {
    Deallocate<SshDriver>(allocator, ssh_driver);
  }
}

void DebugCoordinator::RegisterForPoll(PollTable &table) {
  gdb_async.RegisterForPoll(table);
  vim_async.RegisterForPoll(table);
  if (ssh_driver) {
    ssh_driver->RegisterForPoll(table);
  }
}

void DebugCoordinator::OnPollResults(PollTable &table) {
  gdb_async.OnPollResults(table);
  vim_async.OnPollResults(table);
  if (ssh_driver) {
    ssh_driver->OnPollResults(table);
  }
}

void DebugCoordinator::OnGdbStream(const StringSlice &msg) {
  pdp_info("Received stream message: {}", msg);
}

void DebugCoordinator::OnGdbAsync(GdbAsyncKind kind, UniquePtr<ExprBase> expr) {
  pdp_info("Received async message with kind {}", static_cast<int>(kind));
  PDP_IGNORE(expr);
}

void DebugCoordinator::OnGdbResult(GdbResultKind kind, UniquePtr<ExprBase> expr) {
  pdp_info("Received result message with kind {}", static_cast<int>(kind));
  // GdbExprView dict(expr);
  // StringBuilder builder;
  // dict.ToJson(builder);
  // pdp_info(builder.ToSlice());  // TODO
  // TODO here
  HandleNewBreakpoint(*this, std::move(expr));
  PDP_IGNORE(expr);
}

void DebugCoordinator::OnGdbError(const StringSlice &msg) { pdp_error("GDB error: {}", msg); }

}  // namespace pdp

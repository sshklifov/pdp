#include "debug_coordinator.h"

namespace pdp {

DebugCoordinator::DebugCoordinator(const StringSlice &host, int vim_input_fd, int vim_output_fd,
                                   ChildReaper &reaper)
    : ssh_driver(nullptr), gdb_async(reaper), vim_async(vim_input_fd, vim_output_fd) {
  if (!host.Empty()) {
    ssh_driver = Allocate<SshDriver>(allocator, 1);
    new (ssh_driver) SshDriver(host, reaper);
  }
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

}  // namespace pdp

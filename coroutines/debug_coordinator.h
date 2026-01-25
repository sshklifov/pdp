#pragma once

#include "gdb_async_driver.h"
#include "vim_async_driver.h"

#include "debug_session.h"
#include "drivers/ssh_driver.h"
#include "system/poll_table.h"

namespace pdp {

struct DebugCoordinator {
  DebugCoordinator(const StringSlice &host, int vim_input_fd, int vim_output_fd,
                   ChildReaper &reaper);

  ~DebugCoordinator();

  void RegisterForPoll(PollTable &table);
  void OnPollResults(PollTable &table);

  // bool IsIdle() const;
  // void PrintActivity() const;

  // TODO
  bool IsRemoteDebugging() const { return false; }
  StringSlice GetHost() const { return ""; }

  DebugSession &Session() { return session_data; }

 private:
  DefaultAllocator allocator;
  SshDriver *ssh_driver;

  GdbAsyncDriver gdb_async;
  VimAsyncDriver vim_async;
  DebugSession session_data;
};

}  // namespace pdp

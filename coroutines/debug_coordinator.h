#pragma once

#include "drivers/breakpoint_table.h"
#include "drivers/jump_table.h"
#include "gdb_async_driver.h"
#include "vim_async_driver.h"

#include "drivers/ssh_driver.h"
#include "system/poll_table.h"

namespace pdp {

struct DebugCoordinator {
  DebugCoordinator(const StringSlice &host, int vim_input_fd, int vim_output_fd,
                   ChildReaper &reaper);

  ~DebugCoordinator();

  void RegisterForPoll(PollTable &table);
  void OnPollResults(PollTable &table);

  GdbAsyncDriver &GdbDriver();
  VimAsyncDriver &VimDriver();
  BreakpointTable &Breakpoints();

  pid_t GetInferiorPid() { return inferior_pid; }

  int GetThreadSelected() { return thread_selected; }
  void SetThreadSelected(int tid) { thread_selected = tid; }

  int GetFrameSelected() { return frame_selected; }
  void SetFrameSelected(int frame) { frame_selected = frame; }

  // TODO Do something useful
  void AddThreadId(int64_t id) { PDP_IGNORE(id); }
  void RemoveThreadId(int64_t id) { PDP_IGNORE(id); }

  // TODO Do something useful
  void InsertJump(const StringSlice &fullname, int lnum) {
    PDP_IGNORE(fullname);
    PDP_IGNORE(lnum);
  }

  // bool IsIdle() const;
  // void PrintActivity() const;

  // TODO
  bool IsRemoteDebugging() const { return false; }
  StringSlice GetHost() const { return ""; }

 private:
  DefaultAllocator allocator;
  SshDriver *ssh_driver;

  GdbAsyncDriver gdb_async;
  VimAsyncDriver vim_async;
  BreakpointTable breakpoints;
  JumpTable jump_table;
  Vector<int64_t> thread_ids;

  pid_t inferior_pid;
  int thread_selected;
  int frame_selected;
};

}  // namespace pdp

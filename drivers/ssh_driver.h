#pragma once

#include "data/allocator.h"
#include "data/loop_queue.h"
#include "data/non_copyable.h"
#include "data/small_capture.h"

#include "strings/fixed_string.h"
#include "system/child_reaper.h"
#include "system/file_descriptor.h"
#include "system/poll_table.h"

#include <sys/poll.h>

namespace pdp {

struct SshDriver : public NonCopyableNonMovable {
  using Capture = SmallCapture<FixedString>;

  SshDriver(const StringSlice &host, ChildReaper &reaper);
  ~SshDriver();

  Capture *OnOutput(FixedString request);
  Capture *OnOutput(StringSlice request);

  void RegisterForPoll(PollTable &table);
  void OnPollResults(PollTable &table);

 private:
  void SpawnChildAt(const StringSlice &command, size_t pos);

  void OnChildExited(pid_t pid, int status);

  static void OnChildExited(pid_t pid, int status, void *user_data) {
    static_cast<SshDriver *>(user_data)->OnChildExited(pid, status);
  }

  DefaultAllocator allocator;

  struct PendingOperation {
    PendingOperation(FixedString request) : request(std::move(request)) {}

    FixedString request;
    Capture callback;
  };

  LoopQueue<PendingOperation> pending_queue;

  struct ActiveOperation {
    ActiveOperation() : pid(-1) {}

    pid_t pid;
    InputDescriptor ssh_output;
    InputDescriptor ssh_error;
    StringVector buffer_output;
    StringVector buffer_error;
    Capture cb;
  };

  static constexpr const int max_children = 4;
  ActiveOperation *active_queue;

  FixedString host;
  ChildReaper &reaper;
  struct pollfd *poll_args;
};

}  // namespace pdp

#pragma once

#include "data/allocator.h"
#include "data/loop_queue.h"
#include "data/non_copyable.h"
#include "data/small_capture.h"
#include "data/vector.h"

#include "strings/dynamic_string.h"
#include "system/child_reaper.h"
#include "system/time_units.h"

#include <sys/poll.h>

namespace pdp {

struct SshDriver : public NonCopyableNonMovable {
  using Capture = SmallCapture<DynamicString>;

  SshDriver(const StringSlice &host, ChildReaper &reaper);
  ~SshDriver();

  Capture *OnOutput(DynamicString request);
  Capture *OnOutput(StringSlice request);

  void Poll(Milliseconds timeout);

 private:
  void DispatchAt(const StringSlice &command, size_t pos);

  static void ReadOutput(int fd, Vector<char> &out);

  void OnChildExited(pid_t pid, int status);

  static void OnChildExited(pid_t pid, int status, void *user_data) {
    static_cast<SshDriver *>(user_data)->OnChildExited(pid, status);
  }

  DefaultAllocator allocator;

  struct PendingOperation {
    PendingOperation(DynamicString request) : request(std::move(request)) {}

    DynamicString request;
    Capture callback;
  };

  LoopQueue<PendingOperation> pending_queue;

  struct ActiveOperation {
    ActiveOperation() : output_fd(-1), error_fd(-1), pid(-1) {}

    int output_fd;
    int error_fd;
    pid_t pid;
    Vector<char> output;
    Vector<char> errors;
    Capture cb;
  };

  static constexpr const int max_children = 4;
  ActiveOperation *active_queue;

  DynamicString host;
  ChildReaper &reaper;
  struct pollfd *poll_args;
};

}  // namespace pdp

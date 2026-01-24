#include "ssh_driver.h"

#include <fcntl.h>
#include <sys/poll.h>
#include <sys/prctl.h>
#include <sys/wait.h>
#include <unistd.h>
#include "core/log.h"
#include "system/file_descriptor.h"
#include "tracing/execution_tracer.h"

namespace pdp {

static DynamicString ToDynamicString(Vector<char> &v) {
  v += '\0';
  impl::_VectorPrivAcess<char, DefaultAllocator> _vector_priv(v);

  DynamicString res;
  impl::_DynamicStringPrivInit _string_init(res);
  _string_init(_vector_priv.ReleaseData(), _vector_priv.Get().Size() - 1);
  _vector_priv.Reset();
  return res;
}

SshDriver::SshDriver(const StringSlice &h, ChildReaper &r)
    : pending_queue(max_children, allocator), host(h), reaper(r) {
  active_queue = Allocate<ActiveOperation>(allocator, max_children);
  for (size_t i = 0; i < max_children; ++i) {
    new (active_queue + i) ActiveOperation();
  }
  poll_args = Allocate<struct pollfd>(allocator, max_children * 2);
}

SshDriver::~SshDriver() {
  Deallocate<Capture>(allocator, active_queue);
  Deallocate<pollfd>(allocator, poll_args);
}

SshDriver::Capture *SshDriver::OnOutput(DynamicString request) {
  for (size_t i = 0; i < max_children; ++i) {
    if (active_queue[i].pid < 0) {
      SpawnChildAt(request.ToSlice(), i);
      return &active_queue[i].cb;
    }
  }
  pending_queue.EmplaceBack(std::move(request));
  return &pending_queue.Back().callback;
}

SshDriver::Capture *SshDriver::OnOutput(StringSlice request) {
  for (size_t i = 0; i < max_children; ++i) {
    if (active_queue[i].pid < 0) {
      SpawnChildAt(request, i);
      return &active_queue[i].cb;
    }
  }
  pending_queue.EmplaceBack(DynamicString(request));
  return &pending_queue.Back().callback;
}

void SshDriver::RegisterForPoll(PollTable &table) {
  for (size_t i = 0; i < max_children; ++i) {
    if (active_queue[i].ssh_output.IsValid()) {
      table.Register(active_queue[i].ssh_output.GetDescriptor());
      pdp_assert(active_queue[i].ssh_error.IsValid());
      table.Register(active_queue[i].ssh_error.GetDescriptor());
    }
  }
}

void SshDriver::OnPollResults(PollTable &table) {
  for (size_t i = 0; i < max_children; ++i) {
    if (active_queue[i].ssh_output.IsValid()) {
      if (table.HasInputEvents(active_queue[i].ssh_output.GetDescriptor())) {
        active_queue[i].ssh_output.ReadAvailable(active_queue[i].buffer_output);
      }
      pdp_assert(active_queue[i].ssh_error.IsValid());
      if (table.HasInputEvents(active_queue[i].ssh_error.GetDescriptor())) {
        active_queue[i].ssh_error.ReadAvailable(active_queue[i].buffer_error);
      }
    }
  }
}

void SshDriver::OnChildExited(pid_t pid, int status) {
  for (size_t i = 0; i < max_children; ++i) {
    if (pid == active_queue[i].pid) {
      if (PDP_LIKELY(WIFEXITED(status) && WEXITSTATUS(status) == 0)) {
        active_queue[i].cb(ToDynamicString(active_queue[i].buffer_output));
      } else {
        pdp_error("SSH command failed!");
      }

      if (PDP_UNLIKELY(!active_queue[i].buffer_error.Empty())) {
        StringSlice errors(active_queue[i].buffer_error.Data(),
                           active_queue[i].buffer_error.Size());
        pdp_error_multiline(errors);
      }

      active_queue[i].pid = -1;
      active_queue[i].ssh_output.Close();
      active_queue[i].ssh_error.Close();
      active_queue[i].buffer_output.Clear();
      active_queue[i].buffer_error.Clear();

      if (PDP_UNLIKELY(!pending_queue.Empty())) {
        auto command = std::move(pending_queue.Front().request);
        active_queue[i].cb = pending_queue.Front().callback;
        pending_queue.PopFront();
        SpawnChildAt(command.ToSlice(), i);
      }
      return;
    }
  }
  PDP_UNREACHABLE("Unknown child reaped in SshDriver!");
}

void SshDriver::SpawnChildAt(const StringSlice &command, size_t pos) {
  int out[2], err[2];
  CheckFatal(pipe2(out, O_CLOEXEC), "SSH stdout pipe");
  CheckFatal(pipe2(err, O_CLOEXEC), "SSH stderr pipe");

  pid_t pid = g_recorder.SyscallFork();
  CheckFatal(pid, "SSH fork");

  if (pid == 0) {
    // child
    prctl(PR_SET_PDEATHSIG, SIGTERM);

    int devnull = open("/dev/null", O_RDONLY);
    dup2(devnull, STDIN_FILENO);
    close(devnull);

    dup2(out[1], STDOUT_FILENO);
    close(out[0]);
    close(out[1]);

    dup2(err[1], STDERR_FILENO);
    close(err[0]);
    close(err[1]);

    execlp("ssh", "ssh", "-o", "ConnectTimeout=1", host.Data(), command.Data(), (char *)NULL);
    _exit(127);
  }

  // parent
  close(out[1]);
  close(err[1]);

  active_queue[pos].ssh_output.SetDescriptor(out[0]);
  active_queue[pos].ssh_error.SetDescriptor(err[0]);

  pdp_assert(active_queue[pos].pid == -1);
  active_queue[pos].pid = pid;

  reaper.WatchChild(pid, SshDriver::OnChildExited, this);
}

}  // namespace pdp

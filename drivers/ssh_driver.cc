#include "ssh_driver.h"

#include <fcntl.h>
#include <sys/poll.h>
#include <sys/prctl.h>
#include <sys/wait.h>
#include <unistd.h>
#include <cerrno>
#include "core/log.h"
#include "strings/string_builder.h"
#include "system/file_descriptor.h"

namespace pdp {

static void ReadOutput(int fd, Vector<char> &out) {
  impl::_VectorPrivAcess<char, DefaultAllocator> _vector_priv(out);
  for (;;) {
    _vector_priv.Get().ReserveFor(1024);
    ssize_t ret = read(fd, _vector_priv.Get().End(), _vector_priv.Free());
    if (PDP_UNLIKELY(ret < 0)) {
      if (PDP_LIKELY(errno != EAGAIN && errno != EWOULDBLOCK)) {
        Check(ret, "read");
      }
      return;
    } else if (ret == 0) {
      return;
    }
    _vector_priv.Commit(BitCast<size_t>(ret));
  }
}

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
      SpawnChildAt(request.GetSlice(), i);
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
    if (active_queue[i].output_fd >= 0) {
      table.Register(active_queue[i].output_fd);
      pdp_assert(active_queue[i].error_fd >= 0);
      table.Register(active_queue[i].error_fd);
    }
  }
}

void SshDriver::OnPollResults(PollTable &table) {
  for (size_t i = 0; i < max_children; ++i) {
    if (active_queue[i].output_fd >= 0) {
      if (table.HasInputEvents(active_queue[i].output_fd)) {
        ReadOutput(active_queue[i].output_fd, active_queue[i].output);
      }
      pdp_assert(active_queue[i].error_fd >= 0);
      if (table.HasInputEvents(active_queue[i].error_fd)) {
        ReadOutput(active_queue[i].error_fd, active_queue[i].errors);
      }
    }
  }
}

void SshDriver::OnChildExited(pid_t pid, int status) {
  for (size_t i = 0; i < max_children; ++i) {
    if (pid == active_queue[i].pid) {
      if (PDP_LIKELY(WIFEXITED(status) && WEXITSTATUS(status) == 0)) {
        active_queue[i].cb(ToDynamicString(active_queue[i].output));
      } else {
        pdp_error("SSH command failed!");
      }

      if (PDP_UNLIKELY(!active_queue[i].errors.Empty())) {
        StringSlice errors(active_queue[i].errors.Data(), active_queue[i].errors.Size());
        pdp_error_multiline(errors);
      }

      pdp_assert(active_queue[i].output_fd >= 0);
      close(active_queue[i].output_fd);
      active_queue[i].output_fd = -1;
      pdp_assert(active_queue[i].error_fd >= 0);
      active_queue[i].error_fd = -1;
      active_queue[i].pid = -1;
      active_queue[i].output.Clear();
      active_queue[i].errors.Clear();

      if (PDP_UNLIKELY(!pending_queue.Empty())) {
        auto command = std::move(pending_queue.Front().request);
        active_queue[i].cb = pending_queue.Front().callback;
        pending_queue.PopFront();
        SpawnChildAt(command.GetSlice(), i);
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

  pid_t pid = fork();
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

  SetNonBlocking(out[0]);
  SetNonBlocking(err[0]);

  pdp_assert(active_queue[pos].output_fd == -1);
  active_queue[pos].output_fd = out[0];
  pdp_assert(active_queue[pos].error_fd == -1);
  active_queue[pos].error_fd = err[0];
  pdp_assert(active_queue[pos].pid == -1);
  active_queue[pos].pid = pid;

  reaper.OnChildExited(pid, SshDriver::OnChildExited, this);
}

}  // namespace pdp

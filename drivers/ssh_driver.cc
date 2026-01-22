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
      DispatchAt(request.GetSlice(), i);
      return &active_queue[i].cb;
    }
  }
  pending_queue.EmplaceBack(std::move(request));
  return &pending_queue.Back().callback;
}

SshDriver::Capture *SshDriver::OnOutput(StringSlice request) {
  for (size_t i = 0; i < max_children; ++i) {
    if (active_queue[i].pid < 0) {
      DispatchAt(request, i);
      return &active_queue[i].cb;
    }
  }
  pending_queue.EmplaceBack(DynamicString(request));
  return &pending_queue.Back().callback;
}

void SshDriver::Poll(Milliseconds timeout) {
  pdp_assert(timeout.GetMilli() > 0);
  size_t num_polled = 0;
  for (size_t i = 0; i < max_children; ++i) {
    if (active_queue[i].output_fd >= 0) {
      poll_args[num_polled].fd = active_queue[i].output_fd;
      poll_args[num_polled].events = POLLIN;
      poll_args[num_polled].revents = 0;
      ++num_polled;
      pdp_assert(active_queue[i].error_fd >= 0);
      poll_args[num_polled].fd = active_queue[i].error_fd;
      poll_args[num_polled].events = POLLIN;
      poll_args[num_polled].revents = 0;
      ++num_polled;
    }
  }
  pdp_assert(num_polled <= 2 * max_children);

  if (PDP_LIKELY(num_polled == 0)) {
    return;
  }

  int ret = poll(poll_args, num_polled, timeout.GetMilli());
  if (ret <= 0) {
    Check(ret, "poll");
    return;
  }

  size_t poll_pos = 0;
  for (size_t i = 0; i < max_children; ++i) {
    if (active_queue[i].output_fd == poll_args[poll_pos].fd) {
      if (poll_args[poll_pos].revents & POLLIN) {
        ReadOutput(active_queue[i].output_fd, active_queue[i].output);
      }
      ++poll_pos;
      pdp_assert(poll_args[poll_pos].fd == active_queue[i].error_fd);
      if (poll_args[poll_pos].revents & POLLIN) {
        ReadOutput(active_queue[i].error_fd, active_queue[i].errors);
      }
      ++poll_pos;
    }
  }
  pdp_assert(poll_pos == num_polled);
}

void SshDriver::OnChildExited(pid_t pid, int status) {
  for (size_t i = 0; i < max_children; ++i) {
    if (pid == active_queue[i].pid) {
      if (PDP_LIKELY(WIFEXITED(status) && WEXITSTATUS(status) == 0)) {
        active_queue[i].output += '\0';
        impl::_VectorPrivAcess<char, DefaultAllocator> _vector_priv(active_queue[i].output);

        DynamicString child_output;
        impl::_DynamicStringPrivInit _string_init(child_output);
        _string_init(_vector_priv.ReleaseData(), _vector_priv.Get().Size() - 1);
        _vector_priv.Reset();

        active_queue[i].cb(std::move(child_output));
      } else {
        pdp_error("SSH command failed!");
      }

      if (PDP_UNLIKELY(!active_queue[i].errors.Empty())) {
        StringSlice errors(active_queue[i].errors.Data(), active_queue[i].errors.Size());
        pdp_error_multiline(errors);
        active_queue[i].errors.Clear();
      }

      pdp_assert(active_queue[i].output_fd >= 0);
      close(active_queue[i].output_fd);
      active_queue[i].output_fd = -1;
      pdp_assert(active_queue[i].error_fd >= 0);
      active_queue[i].error_fd = -1;

      active_queue[i].pid = -1;

      if (PDP_UNLIKELY(!pending_queue.Empty())) {
        auto command = std::move(pending_queue.Front().request);
        active_queue[i].cb = pending_queue.Front().callback;
        pending_queue.PopFront();
        DispatchAt(command.GetSlice(), i);
      }
      return;
    }
  }
  PDP_UNREACHABLE("Unknown child reaped in SshDriver!");
}

void SshDriver::DispatchAt(const StringSlice &command, size_t pos) {
  int out[2], err[2];
  CheckFatal(pipe(out), "SSH stdout pipe");
  CheckFatal(pipe(err), "SSH stderr pipe");

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

void SshDriver::ReadOutput(int fd, Vector<char> &out) {
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

}  // namespace pdp

#include "gdb_session.h"
#include "check.h"
#include "log.h"

#include <fcntl.h>
#include <poll.h>
#include <unistd.h>

namespace pdp {

GdbSession::GdbSession(Callback async_callback, Callback stream_callback)
    : in{0, 0}, out{0, 0}, err{0, 0}, token_counter(0), disconnected(false) {}

GdbSession::~GdbSession() {
  if (in[1] > 0) {
    close(in[1]);
  }
  if (out[0] > 0) {
    close(out[0]);
  }
  if (err[1] > 0) {
    close(err[1]);
  }
}

void GdbSession::Start() {
  pipe(in);
  pipe(out);
  pipe(err);

  pid_t pid = fork();
  if (pid == 0) {
    // child: GDB
    dup2(in[0], STDIN_FILENO);
    dup2(out[1], STDOUT_FILENO);
    dup2(err[1], STDERR_FILENO);

    close(in[1]);
    close(out[0]);
    close(err[0]);

    int ret = execlp("gdb", "gdb", "--interpreter=mi2", nullptr);
    Check(ret, "execlp");
    _exit(1);
  }

  // parent
  close(in[0]);
  close(out[1]);
  close(err[1]);

  int flags = fcntl(out[0], F_GETFL, 0);
  int ret = fcntl(out[0], F_SETFL, flags | O_NONBLOCK);
  CheckAndTerminate(ret, "fcntl");
}

void GdbSession::SendCommand(const StringView &command, Callback cb) {
  StringBuilder builder;
  builder.Append(token_counter);
  builder.Append(command);
  builder.Append('\n');

  callbacks.push_back(std::move(cb));
  ++token_counter;

  ssize_t total_written = 0;
  ssize_t remaining = command.Size();
  while (remaining > 0) {
    ssize_t ret = write(in[1], builder.Data() + total_written, remaining);
    if (ret < 0) {
      Check(ret, "write");
      std::terminate();
    }
    total_written += ret;
    remaining -= ret;
  }
}

void GdbSession::Poll(std::chrono::milliseconds ms) {
  struct pollfd poll_args;
  poll_args.fd = out[0];
  poll_args.events = POLLIN;

  int ret = poll(&poll_args, 1, ms.count());
  if (ret <= 0) {
    Check(ret, "poll");
    return;
  }

  if (poll_args.revents & (POLLHUP | POLLERR)) {
    pdp_error("GDB session disconnected!");
    disconnected = true;
    return;
  }

  if (poll_args.revents & POLLIN) {
    Process();
  }
}

void GdbSession::Process() {
  size_t n = buffer.ReadFull(out[0]);
  if (n <= 0) {
    pdp_warning("Could not read data after polling!");
    return;
  }

  auto s = buffer.ConsumeLine();
  while (!s.Empty()) {
    pdp_info("Read: {}", s);
    s = buffer.ConsumeLine();
  }
}

};  // namespace pdp

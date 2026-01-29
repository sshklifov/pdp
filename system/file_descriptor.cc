#include "file_descriptor.h"

#include "core/check.h"
#include "tracing/execution_tracer.h"

#include <fcntl.h>
#include <linux/limits.h>
#include <sys/poll.h>
#include <unistd.h>
#include <cerrno>
#include <ctime>

namespace pdp {

int DuplicateForThisProcess(int fd) {
  int dupped = fcntl(fd, F_DUPFD_CLOEXEC, 0);
  Check(dupped, "fcntl::dupfd");
  return dupped;
}

void SetNonBlocking(int fd) {
  int flags = fcntl(fd, F_GETFL, 0);
  int ret = fcntl(fd, F_SETFL, flags | O_NONBLOCK);
  if (PDP_UNLIKELY(!Check(ret, "fcntl"))) {
    PDP_UNREACHABLE("Cannot setup non-blocking IO");
  }
}

FixedString RealPath(const char *relative) {
  StringBuffer absolute(PATH_MAX + 1);
  Check(realpath(relative, absolute.Get()), "realpath");
  size_t length = strlen(absolute.Get());
  absolute.ShrinkToFit(length + 1);
  return FixedString(std::move(absolute), length);
}

bool FileReadable(const char *file) { return access(file, R_OK) == 0; }

FileDescriptor::FileDescriptor() : fd(-1) {}

FileDescriptor::FileDescriptor(int descriptor) : fd(descriptor) { SetNonBlocking(fd); }

FileDescriptor::~FileDescriptor() { Check(close(fd), "close"); }

bool FileDescriptor::IsValid() const { return fd >= 0; }

int FileDescriptor::GetDescriptor() const { return fd; }

void FileDescriptor::Close() {
  pdp_assert(IsValid());
  Check(close(fd), "FileDescriptor::Close");
  fd = -1;
}

void FileDescriptor::SetDescriptor(int init_fd) {
  pdp_assert(fd < 0);
  fd = init_fd;
  SetNonBlocking(fd);
}

bool FileDescriptor::WaitForEvents(int events, Milliseconds timeout) {
  pdp_assert(timeout.Get() > 0);
  struct pollfd poll_args;
  poll_args.fd = fd;
  poll_args.events = events;
  poll_args.revents = 0;

  int ret = g_recorder.SyscallPoll(&poll_args, 1, timeout.Get());
  if (ret <= 0) {
    Check(ret, "poll");
    return false;
  }
  return (poll_args.revents & events);
}

bool InputDescriptor::WaitForInput(Milliseconds timeout) { return WaitForEvents(POLLIN, timeout); }

size_t InputDescriptor::ReadAtLeast(void *buf, size_t required_bytes, size_t free_bytes,
                                    Milliseconds timeout) {
  pdp_assert(required_bytes > 0);
  pdp_assert(required_bytes <= free_bytes);

  Stopwatch stopwatch;
  size_t num_read = 0;
  for (;;) {
    size_t n = 0;
    do {
      n = ReadOnce((char *)buf + num_read, free_bytes - num_read);
      num_read += n;
      if (num_read >= required_bytes) {
        return num_read;
      }
    } while (n != 0);
    Milliseconds wait = timeout - stopwatch.Elapsed();
    if (g_recorder.IsTimeLess(wait, 1_ms)) {
      return num_read;
    }
    if (!WaitForInput(wait)) {
      return num_read;
    }
  }
}

bool InputDescriptor::ReadExactly(void *buf, size_t size, Milliseconds timeout) {
  size_t num_read = ReadAtLeast(buf, size, size, timeout);
  pdp_assert(num_read <= size);
  return num_read == size;
}

size_t InputDescriptor::ReadAvailable(void *buf, size_t max_bytes) {
  pdp_assert(max_bytes > 0);
  size_t num_read = 0;
  do {
    size_t s = ReadOnce((char *)buf + num_read, max_bytes - num_read);
    if (s == 0) {
      return num_read;
    }
    num_read += s;
  } while (num_read < max_bytes);
  pdp_assert(num_read == max_bytes);
  return num_read;
}

size_t InputDescriptor::ReadAvailable(StringVector &out) {
  size_t num_read = 0;
  for (;;) {
    out.ReserveFor(1024);
    size_t ret = ReadOnce(out.End(), out.Free());
    if (ret <= 0) {
      return num_read;
    }
    out.MoveEnd(ret);
    num_read += ret;
  }
}

size_t InputDescriptor::ReadOnce(void *buf, size_t size) {
  pdp_assert(size > 0);
  ssize_t ret = g_recorder.SyscallRead(fd, buf, size);
  if (ret <= 0) {
    if (PDP_UNLIKELY(errno != EAGAIN && errno != EWOULDBLOCK)) {
      Check(ret, "read");
    }
    return 0;
  }
  return static_cast<size_t>(ret);
}

bool OutputDescriptor::WaitForOutput(Milliseconds timeout) {
  return WaitForEvents(POLLOUT, timeout);
}

bool OutputDescriptor::WriteExactly(const void *buf, size_t bytes, Milliseconds timeout) {
  pdp_assert(bytes > 0);

  Stopwatch stopwatch;
  size_t num_written = 0;
  for (;;) {
    size_t n = 0;
    do {
      n = WriteOnce((char *)buf + num_written, bytes - num_written);
      num_written += n;
      if (num_written >= bytes) {
        pdp_assert(num_written == bytes);
        return true;
      }
    } while (n != 0);
    Milliseconds wait = timeout - stopwatch.Elapsed();
    if (g_recorder.IsTimeLess(wait, 1_ms)) {
      return false;
    }
    if (!WaitForOutput(wait)) {
      return false;
    }
  }
}

size_t OutputDescriptor::WriteOnce(const void *buf, size_t size) {
  pdp_assert(size > 0);
  ssize_t ret = g_recorder.SyscallWrite(fd, buf, size);
  if (ret <= 0) {
    if (PDP_UNLIKELY(errno != EAGAIN && errno != EWOULDBLOCK)) {
      Check(ret, "read");
    }
    return 0;
  }
  return BitCast<size_t>(ret);
}

}  // namespace pdp

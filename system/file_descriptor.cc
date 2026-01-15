#include "file_descriptor.h"

#include "core/check.h"
#include "strings/string_builder.h"

#include <fcntl.h>
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

FileDescriptor::FileDescriptor() : fd(-1) {}

FileDescriptor::FileDescriptor(int descriptor) : fd(descriptor) { SetNonBlocking(); }

FileDescriptor::~FileDescriptor() { Check(close(fd), "close"); }

bool FileDescriptor::IsValid() const { return fd >= 0; }

int FileDescriptor::Value() const { return fd; }

void FileDescriptor::SetDescriptor(int init_fd) {
  pdp_assert(fd < 0);
  fd = init_fd;
  SetNonBlocking();
}

bool FileDescriptor::WaitForEvents(int events, Milliseconds timeout) {
  pdp_assert(timeout.GetMilli() > 0);
  struct pollfd poll_args;
  poll_args.fd = fd;
  poll_args.events = events;

  int ret = poll(&poll_args, 1, timeout.GetMilli());
  if (ret <= 0) {
    Check(ret, "poll");
    return false;
  }
  return (poll_args.revents & events);
}

void FileDescriptor::SetNonBlocking() {
  int flags = fcntl(fd, F_GETFL, 0);
  int ret = fcntl(fd, F_SETFL, flags | O_NONBLOCK);
  if (PDP_UNLIKELY(!Check(ret, "fcntl"))) {
    PDP_UNREACHABLE("Cannot setup non-blocking IO");
  }
}

bool InputDescriptor::WaitForInput(Milliseconds timeout) { return WaitForEvents(POLLIN, timeout); }

size_t InputDescriptor::ReadAtLeast(void *buf, size_t required_bytes, size_t free_bytes,
                                    Milliseconds timeout) {
  pdp_assert(required_bytes > 0);
  pdp_assert(required_bytes <= free_bytes);

  Milliseconds next_wait = timeout;
  Stopwatch stopwatch;

  size_t num_read = 0;
  while (num_read < required_bytes && next_wait > 0_ms) {
    WaitForInput(next_wait > 5_ms ? next_wait : 5_ms);
    size_t n = 0;
    do {
      n = ReadOnce((char *)buf + num_read, free_bytes - num_read);
      num_read += n;
      if (num_read >= required_bytes) {
        return num_read;
      }
    } while (n != 0);
    next_wait = timeout - stopwatch.ElapsedMilli();
  }
  return num_read;
}

bool InputDescriptor::ReadExactly(void *buf, size_t size, Milliseconds timeout) {
  size_t num_read = ReadAtLeast(buf, size, size, timeout);
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

size_t InputDescriptor::ReadOnce(void *buf, size_t size) {
  pdp_assert(size > 0);
  ssize_t ret = read(fd, buf, size);
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

  Milliseconds next_wait = timeout;
  Stopwatch stopwatch;

  size_t num_written = 0;
  while (num_written < bytes && next_wait > 0_ms) {
    WaitForOutput(next_wait > 5_ms ? next_wait : 5_ms);
    size_t n = 0;
    do {
      n = WriteOnce((char *)buf + num_written, bytes - num_written);
      num_written += n;
      if (num_written >= bytes) {
        return true;
      }
    } while (n != 0);
    next_wait = timeout - stopwatch.ElapsedMilli();
  }
  return num_written == bytes;
}

size_t OutputDescriptor::WriteOnce(const void *buf, size_t size) {
  pdp_assert(size > 0);
  ssize_t ret = write(fd, buf, size);
  if (ret <= 0) {
    if (PDP_UNLIKELY(errno != EAGAIN && errno != EWOULDBLOCK)) {
      Check(ret, "read");
    }
    return 0;
  }
  return BitCast<size_t>(ret);
}

}  // namespace pdp

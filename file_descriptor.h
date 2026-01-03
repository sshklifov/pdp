#pragma once

#include "core/check.h"
#include "core/log.h"

#include <fcntl.h>
#include <sys/poll.h>
#include <unistd.h>
#include <cerrno>
#include <ctime>

namespace pdp {

struct InputDescriptor {
  InputDescriptor() : fd(-1) {}
  InputDescriptor(int descriptor) : fd(descriptor) {
    pdp_assert(fd >= 0);
    SetNonBlocking();
  }

  InputDescriptor(InputDescriptor &&) = delete;
  InputDescriptor(const InputDescriptor &) = delete;

  void operator=(const InputDescriptor &) = delete;
  void operator=(InputDescriptor &&) = delete;

  ~InputDescriptor() {
    pdp_assert(fd >= 0);
    if (fd >= 0) {
      Check(close(fd), "close");
    }
  }

  int Id() const { return fd; }

  void SetId(int init_fd) {
    pdp_assert(fd < 0);
    pdp_assert(init_fd >= 0);
    fd = init_fd;
    SetNonBlocking();
  }

  bool WaitForInput(unsigned milliseconds) {
    struct pollfd poll_args;
    poll_args.fd = fd;
    poll_args.events = POLLIN;

    int ret = poll(&poll_args, 1, milliseconds);
    if (ret <= 0) {
      Check(ret, "poll");
      return false;
    }

    if (poll_args.revents & (POLLHUP | POLLERR)) {
      pdp_critical("Fatal: POLLHUP/POLLERR on critical resource!");
      PDP_UNREACHABLE();
    }

    return (poll_args.revents & POLLIN);
  }

  size_t ReadAtLeast(void *buf, size_t required_bytes, size_t free_bytes, int timeout_ms) {
    pdp_assert(required_bytes > 0);
    pdp_assert(required_bytes <= free_bytes);

    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);

    size_t num_read = 0;
    int next_wait_ms = timeout_ms;
    while (num_read < required_bytes && next_wait_ms > 0) {
      const int min_wait_ms = 5;
      WaitForInput(next_wait_ms > min_wait_ms ? next_wait_ms : min_wait_ms);
      size_t n = 0;
      do {
        n = Read((char *)buf + num_read, free_bytes - num_read);
        num_read += n;
      } while (n != 0);
      struct timespec now;
      clock_gettime(CLOCK_MONOTONIC, &now);
      int64_t elapsed_ms =
          (now.tv_sec - ts.tv_sec) * 1'000 + (now.tv_nsec - ts.tv_nsec) / 1'000'000;
      next_wait_ms = timeout_ms - elapsed_ms;
    }
    return num_read;
  }

  bool ReadExactly(void *buf, size_t size, int timeout_ms) {
    size_t num_read = ReadAtLeast(buf, size, size, timeout_ms);
    return num_read == size;
  }

 private:
  void SetNonBlocking() {
    int flags = fcntl(fd, F_GETFL, 0);
    int ret = fcntl(fd, F_SETFL, flags | O_NONBLOCK);
    CheckAndTerminate(ret, "fcntl");
  }

  size_t Read(void *buf, size_t size) {
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

  int fd;
};

struct OutputDescriptor {
  OutputDescriptor() : fd(-1) {}
  OutputDescriptor(int descriptor) : fd(descriptor) {
    pdp_assert(fd >= 0);
    SetNonBlocking();
  }

  OutputDescriptor(OutputDescriptor &&) = delete;
  OutputDescriptor(const OutputDescriptor &) = delete;

  void operator=(const OutputDescriptor &) = delete;
  void operator=(OutputDescriptor &&) = delete;

  ~OutputDescriptor() {
    pdp_assert(fd >= 0);
    if (fd >= 0) {
      Check(close(fd), "close");
    }
  }

  void SetId(int init_fd) {
    pdp_assert(fd < 0);
    pdp_assert(init_fd >= 0);
    fd = init_fd;
    SetNonBlocking();
  }

  int Id() const { return fd; }

  bool WaitForOutput(unsigned milliseconds) {
    struct pollfd poll_args;
    poll_args.fd = fd;
    poll_args.events = POLLOUT;

    int ret = poll(&poll_args, 1, milliseconds);
    if (ret <= 0) {
      Check(ret, "poll");
      return false;
    }

    if (poll_args.revents & (POLLHUP | POLLERR)) {
      pdp_critical("Fatal: POLLHUP/POLLERR on critical resource!");
      PDP_UNREACHABLE();
    }

    return (poll_args.revents & POLLOUT);
  }

  bool WriteExactly(void *buf, size_t bytes, int timeout_ms) {
    pdp_assert(bytes > 0);

    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);

    size_t num_written = 0;
    int next_wait_ms = timeout_ms;
    while (num_written < bytes && next_wait_ms > 0) {
      const int min_wait_ms = 5;
      WaitForOutput(next_wait_ms > min_wait_ms ? next_wait_ms : min_wait_ms);
      size_t n = 0;
      do {
        n = Write((char *)buf + num_written, bytes - num_written);
        num_written += n;
      } while (n != 0);
      struct timespec now;
      clock_gettime(CLOCK_MONOTONIC, &now);
      int64_t elapsed_ms =
          (now.tv_sec - ts.tv_sec) * 1'000 + (now.tv_nsec - ts.tv_nsec) / 1'000'000;
      next_wait_ms = timeout_ms - elapsed_ms;
    }
    return num_written == bytes;
  }

 private:
  void SetNonBlocking() {
    int flags = fcntl(fd, F_GETFL, 0);
    int ret = fcntl(fd, F_SETFL, flags | O_NONBLOCK);
    CheckAndTerminate(ret, "fcntl");
  }

  size_t Write(void *buf, size_t size) {
    pdp_assert(size > 0);
    ssize_t ret = write(fd, buf, size);
    if (ret <= 0) {
      if (PDP_UNLIKELY(errno != EAGAIN && errno != EWOULDBLOCK)) {
        Check(ret, "read");
      }
      return 0;
    }
    return static_cast<size_t>(ret);
  }

  int fd;
};

}  // namespace pdp

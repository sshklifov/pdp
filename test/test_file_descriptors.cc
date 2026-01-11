#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "system/file_descriptor.h"

#include <fcntl.h>
#include <unistd.h>
#include <chrono>
#include <cstring>
#include <thread>

using namespace pdp;

// -----------------------------------------------------------------------------
// Test helper: RAII pipe
// -----------------------------------------------------------------------------
struct Pipe {
  int rfd = -1;
  int wfd = -1;

  Pipe() {
    int fds[2];
    REQUIRE(pipe(fds) == 0);
    rfd = fds[0];
    wfd = fds[1];
  }

  ~Pipe() {
    if (rfd >= 0) close(rfd);
    if (wfd >= 0) close(wfd);
  }
};

// -----------------------------------------------------------------------------
// FileDescriptor basics
// -----------------------------------------------------------------------------
TEST_CASE("FileDescriptor default state and ownership") {
  FileDescriptor fd;
  CHECK(!fd.IsValid());

  int fds[2];
  REQUIRE(pipe(fds) == 0);

  {
    FileDescriptor owned(fds[0]);
    CHECK(owned.IsValid());
    CHECK(owned.Value() == fds[0]);
  }

  CHECK(close(fds[0]) < 0);
  fd.SetDescriptor(fds[1]);
}

// -----------------------------------------------------------------------------
// SetValue + non-blocking enforcement
// -----------------------------------------------------------------------------
TEST_CASE("FileDescriptor SetValue sets non-blocking flag") {
  FileDescriptor fd;

  int fds[2];
  REQUIRE(pipe(fds) == 0);

  fd.SetDescriptor(fds[0]);
  CHECK(fd.IsValid());
  CHECK(fd.Value() == fds[0]);

  int flags = fcntl(fd.Value(), F_GETFL, 0);
  bool is_nonblock = flags & O_NONBLOCK;
  CHECK(is_nonblock);

  close(fds[1]);
}

// -----------------------------------------------------------------------------
// WaitForInput detects readiness
// -----------------------------------------------------------------------------
TEST_CASE("InputDescriptor WaitForInput wakes on incoming data") {
  Pipe p;
  InputDescriptor in(p.rfd);

  std::thread writer([&] {
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    char c = 'x';
    REQUIRE(write(p.wfd, &c, 1) == 1);
  });

  bool ready = in.WaitForInput(500_ms);
  CHECK(ready);

  writer.join();
}

// -----------------------------------------------------------------------------
// ReadAtLeast basic behavior
// -----------------------------------------------------------------------------
TEST_CASE("InputDescriptor ReadAtLeast reads required bytes") {
  Pipe p;
  InputDescriptor in(p.rfd);

  const char payload[] = "hello world";
  REQUIRE(write(p.wfd, payload, sizeof(payload)) == sizeof(payload));

  char buf[32];
  size_t n = in.ReadAtLeast(buf, 5, sizeof(buf), 500_ms);

  CHECK(n >= 5);
  CHECK(std::memcmp(buf, "hello", 5) == 0);
}

// -----------------------------------------------------------------------------
// ReadExactly success case
// -----------------------------------------------------------------------------
TEST_CASE("InputDescriptor ReadExactly succeeds when data available") {
  Pipe p;
  InputDescriptor in(p.rfd);

  const char payload[] = "abcdefghijklmnop";
  REQUIRE(write(p.wfd, payload, sizeof(payload)) == sizeof(payload));

  char buf[8];
  memset(buf, 0, sizeof(buf));
  bool ok = in.ReadExactly(buf, 6, 500_ms);

  CHECK(ok);
  CHECK(std::memcmp(buf, "abcdef", 6) == 0);
  CHECK(buf[6] == 0);
  CHECK(buf[7] == 0);
}

// -----------------------------------------------------------------------------
// ReadExactly timeout failure
// -----------------------------------------------------------------------------
TEST_CASE("InputDescriptor ReadExactly fails on timeout") {
  Pipe p;
  InputDescriptor in(p.rfd);

  char buf[8];
  bool ok = in.ReadExactly(buf, 4, 50_ms);
  CHECK(!ok);
}

// -----------------------------------------------------------------------------
// OutputDescriptor WriteExactly round-trip
// -----------------------------------------------------------------------------
TEST_CASE("OutputDescriptor WriteExactly writes full buffer") {
  Pipe p;
  OutputDescriptor out(p.wfd);
  InputDescriptor in(p.rfd);

  const char payload[] = "ping";
  bool ok = out.WriteExactly((void *)payload, sizeof(payload), 500_ms);
  CHECK(ok);

  char buf[16] = {};
  size_t n = in.ReadAtLeast(buf, sizeof(payload), sizeof(buf), 500_ms);

  CHECK(n == sizeof(payload));
  CHECK(std::memcmp(buf, payload, sizeof(payload)) == 0);
}

// -----------------------------------------------------------------------------
// Partial write handling
// -----------------------------------------------------------------------------
TEST_CASE("OutputDescriptor multiple writes are read as one contiguous buffer") {
  Pipe p;
  OutputDescriptor out(p.wfd);
  InputDescriptor in(p.rfd);

  const char part1[] = "hello ";
  const char part2[] = "world";

  // Write in two separate calls
  REQUIRE(out.WriteExactly((void *)part1, sizeof(part1) - 1, 500_ms));
  REQUIRE(out.WriteExactly((void *)part2, sizeof(part2) - 1, 500_ms));

  // Read everything in a single call
  char buf[32];
  size_t n = in.ReadAtLeast(buf, (sizeof(part1) - 1) + (sizeof(part2) - 1), sizeof(buf), 500_ms);

  REQUIRE(n == (sizeof(part1) - 1) + (sizeof(part2) - 1));
  CHECK(std::memcmp(buf, "hello world", n) == 0);
}

TEST_CASE("OutputDescriptor delayed second write from another thread is read contiguously") {
  Pipe p;
  OutputDescriptor out(p.wfd);
  InputDescriptor in(p.rfd);

  const char part1[] = "this is a ";
  const char part2[] = "delayed message";

  // First write happens immediately
  REQUIRE(out.WriteExactly((void *)part1, sizeof(part1) - 1, 500_ms));

  // Second write happens later from another thread
  std::thread delayed_writer([&] {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    REQUIRE(out.WriteExactly((void *)part2, sizeof(part2) - 1, 500_ms));
  });

  // Single read that must span both writes
  char buf[32] = {};
  size_t expected = (sizeof(part1) - 1) + (sizeof(part2) - 1);

  size_t n = in.ReadAtLeast(buf, expected, sizeof(buf), 500_ms);

  REQUIRE(n == expected);
  CHECK(std::memcmp(buf, "this is a delayed message", n) == 0);

  delayed_writer.join();
}

TEST_CASE("InputDescriptor ReadAtLeast times out if delayed second write arrives too late") {
  Pipe p;
  OutputDescriptor out(p.wfd);
  InputDescriptor in(p.rfd);

  const char part1[] = "Hello from ";
  const char part2[] = "the upside down";

  // First part is written immediately.
  REQUIRE(out.WriteExactly((void *)part1, sizeof(part1) - 1, 500_ms));

  // Second part is written too late (after the read timeout).
  std::thread delayed_writer([&] {
    std::this_thread::sleep_for(std::chrono::milliseconds(350));
    REQUIRE(out.WriteExactly((void *)part2, sizeof(part2) - 1, 500_ms));
  });

  char buf[32];
  const size_t required = (sizeof(part1) - 1) + (sizeof(part2) - 1);

  // Timeout deliberately smaller than the writer delay.
  const Milliseconds timeout = 120_ms;

  size_t n = in.ReadAtLeast(buf, required, sizeof(buf), timeout);

  // Must fail to reach "required" bytes.
  CHECK(n < required);

  // But it should at least contain the first part (very likely exactly that).
  CHECK(n >= (sizeof(part1) - 1));
  CHECK(std::memcmp(buf, part1, sizeof(part1) - 1) == 0);

  delayed_writer.join();
}

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <fcntl.h>
#include <sys/poll.h>
#include <unistd.h>
#include <thread>

#include "strings/rolling_buffer.h"

using pdp::Milliseconds;
using pdp::RollingBuffer;
using pdp::Stopwatch;
using pdp::StringSlice;

constexpr pdp::Milliseconds timeout(100);

namespace {

pdp::StringSlice ReadWithTimeout(RollingBuffer &buffer, Milliseconds timeout) {
  struct pollfd poll_args;
  poll_args.fd = buffer.GetDescriptor();
  poll_args.events = POLLIN;
  poll_args.revents = 0;

  Stopwatch stopw;
  auto next_wait = timeout;
  while (next_wait.GetMilli() > 0) {
    poll(&poll_args, 1, timeout.GetMilli());
    auto m = buffer.ReadLine();
    if (m.end - m.begin > 1) {
      return StringSlice(m.begin, m.end);
    }
    next_wait = timeout - stopw.Elapsed();
  }
  return StringSlice("");
}

int MakePipe(int fds[2]) {
  int rc = pipe(fds);
  REQUIRE(rc == 0);
  return rc;
}

int MakeNonBlock(int fds[2]) {
  int flags = fcntl(fds[0], F_GETFL, 0);
  REQUIRE(flags != -1);

  int rc = fcntl(fds[0], F_SETFL, flags | O_NONBLOCK);
  REQUIRE(rc != -1);
  return rc;
}

void WriteAll(int fd, const char *data, size_t size) {
  size_t written = 0;
  while (written < size) {
    ssize_t rc = write(fd, data + written, size - written);
    REQUIRE(rc > 0);
    written += rc;
  }
}

void WriteAll(int fd, const char *data) { WriteAll(fd, data, strlen(data)); }

}  // namespace

TEST_CASE("RollingBuffer: single short line") {
  int fds[2];
  MakePipe(fds);

  WriteAll(fds[1], "hello\n");
  close(fds[1]);

  RollingBuffer buf;
  buf.SetDescriptor(fds[0]);
  StringSlice line = ReadWithTimeout(buf, timeout);

  CHECK(line == "hello\n");
}

TEST_CASE("RollingBuffer: multiple lines in one write") {
  int fds[2];
  MakePipe(fds);

  const char *data = "a\nb\nc\n";
  WriteAll(fds[1], data);
  close(fds[1]);

  RollingBuffer buf;
  buf.SetDescriptor(fds[0]);

  auto line = ReadWithTimeout(buf, timeout);
  CHECK(line == "a\n");
  line = ReadWithTimeout(buf, timeout);
  CHECK(line == "b\n");
  line = ReadWithTimeout(buf, timeout);
  CHECK(line == "c\n");
}

TEST_CASE("RollingBuffer: line split across reads") {
  int fds[2];
  MakePipe(fds);

  WriteAll(fds[1], "hello ");
  WriteAll(fds[1], "world\n");
  close(fds[1]);

  RollingBuffer buf;
  buf.SetDescriptor(fds[0]);
  auto line = ReadWithTimeout(buf, timeout);

  CHECK(line == "hello world\n");
}

TEST_CASE("RollingBuffer: empty input returns empty slice") {
  int fds[2];
  MakePipe(fds);
  close(fds[1]);

  RollingBuffer buf;
  buf.SetDescriptor(fds[0]);
  auto line = ReadWithTimeout(buf, timeout);

  CHECK(line.Empty());
}

TEST_CASE("RollingBuffer: long line > default buffer size") {
  int fds[2];
  MakePipe(fds);

  const size_t line_size = RollingBuffer::default_buffer_size + 1024;

  std::thread producer([fds]() {
    char *data = new char[line_size];
    memset(data, 'x', line_size - 1);
    data[line_size - 1] = '\n';
    WriteAll(fds[1], data, line_size);
    delete[] data;
  });

  RollingBuffer buf;
  buf.SetDescriptor(fds[0]);

  pdp::Stopwatch watch;
  auto line = ReadWithTimeout(buf, timeout);

  CHECK(line.Size() == line_size);
  CHECK(line.Find('\n') == line.End() - 1);

  producer.join();

  close(fds[1]);
}

TEST_CASE("RollingBuffer: many small lines trigger rollover") {
  int fds[2];
  MakePipe(fds);

  RollingBuffer buf;
  buf.SetDescriptor(fds[0]);
  const int lines = 10'000;

  std::thread producer([fds]() {
    for (int i = 0; i < lines; ++i) {
      WriteAll(fds[1], "Test line\n");
    }
  });

  for (int i = 0; i < lines; ++i) {
    auto s = ReadWithTimeout(buf, timeout);
    CHECK(s == "Test line\n");
  }

  producer.join();

  close(fds[1]);
}

TEST_CASE("RollingBuffer: final line without newline") {
  int fds[2];
  MakePipe(fds);

  WriteAll(fds[1], "no_newline");
  close(fds[1]);

  RollingBuffer buf;
  buf.SetDescriptor(fds[0]);
  auto line = ReadWithTimeout(buf, timeout);

  CHECK(line.Empty());
}

TEST_CASE("RollingBuffer: wait for newline") {
  int fds[2];
  MakePipe(fds);
  MakeNonBlock(fds);

  RollingBuffer buf;
  buf.SetDescriptor(fds[0]);

  WriteAll(fds[1], "No newline");

  auto line = ReadWithTimeout(buf, timeout);
  CHECK(line.Empty());

  WriteAll(fds[1], " and still nothing");
  line = ReadWithTimeout(buf, timeout);
  CHECK(line.Empty());

  WriteAll(fds[1], " but then\nThere is light\n");
  close(fds[1]);

  line = ReadWithTimeout(buf, timeout);
  CHECK(line == "No newline and still nothing but then\n");

  line = ReadWithTimeout(buf, timeout);
  CHECK(line == "There is light\n");
}

TEST_CASE("RollingBuffer: nonblocking behavior") {
  int fds[2];
  MakePipe(fds);
  MakeNonBlock(fds);

  RollingBuffer buf;
  buf.SetDescriptor(fds[0]);

  WriteAll(fds[1], "Final thing.\n");

  auto line = ReadWithTimeout(buf, timeout);
  CHECK(line == "Final thing.\n");

  line = ReadWithTimeout(buf, timeout);
  CHECK(line.Empty());

  close(fds[1]);
}

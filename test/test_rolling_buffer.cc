#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <fcntl.h>
#include <unistd.h>
#include <thread>

#include "strings/rolling_buffer.h"

using pdp::RollingBuffer;
using pdp::StringSlice;

namespace {

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
  StringSlice line = buf.ReadLine(fds[0]);

  CHECK(line == "hello\n");

  close(fds[0]);
}

TEST_CASE("RollingBuffer: multiple lines in one write") {
  int fds[2];
  MakePipe(fds);

  const char *data = "a\nb\nc\n";
  WriteAll(fds[1], data);
  close(fds[1]);

  RollingBuffer buf;

  auto line = buf.ReadLine(fds[0]);
  CHECK(line == "a\n");
  line = buf.ReadLine(fds[0]);
  CHECK(line == "b\n");
  line = buf.ReadLine(fds[0]);
  CHECK(line == "c\n");

  close(fds[0]);
}

TEST_CASE("RollingBuffer: line split across reads") {
  int fds[2];
  MakePipe(fds);

  WriteAll(fds[1], "hello ");
  WriteAll(fds[1], "world\n");
  close(fds[1]);

  RollingBuffer buf;
  auto line = buf.ReadLine(fds[0]);

  CHECK(line == "hello world\n");
  close(fds[0]);
}

TEST_CASE("RollingBuffer: empty input returns empty slice") {
  int fds[2];
  MakePipe(fds);
  close(fds[1]);

  RollingBuffer buf;
  auto line = buf.ReadLine(fds[0]);

  CHECK(line.Empty());
  close(fds[0]);
}

TEST_CASE("RollingBuffer: long line > default buffer size") {
  int fds[2];
  MakePipe(fds);

  const size_t line_size = RollingBuffer::default_buffer_size + 1024;

  std::thread producer([fds](){
    char *data = new char[line_size];
    memset(data, 'x', line_size - 1);
    data[line_size - 1] = '\n';

    WriteAll(fds[1], data, line_size);
    delete[] data;
    });

  RollingBuffer buf;
  auto line = buf.ReadLine(fds[0]);

  CHECK(line.Size() == line_size);
  CHECK(line.Find('\n') == line.End() - 1);

  producer.join();

  close(fds[0]);
  close(fds[1]);
}

TEST_CASE("RollingBuffer: many small lines trigger rollover") {
  int fds[2];
  MakePipe(fds);

  RollingBuffer buf;
  const int lines = 10'000;

  std::thread producer([fds]() {
    for (int i = 0; i < lines; ++i) {
      WriteAll(fds[1], "Test line\n");
    }
  });

  for (int i = 0; i < lines; ++i) {
    auto s = buf.ReadLine(fds[0]);
    CHECK(s == "Test line\n");
  }

  producer.join();

  close(fds[0]);
  close(fds[1]);
}

TEST_CASE("RollingBuffer: final line without newline") {
  int fds[2];
  MakePipe(fds);

  WriteAll(fds[1], "no_newline");
  close(fds[1]);

  RollingBuffer buf;
  auto line = buf.ReadLine(fds[0]);

  CHECK(line.Empty());
  close(fds[0]);
}

TEST_CASE("RollingBuffer: wait for newline") {
  int fds[2];
  MakePipe(fds);
  MakeNonBlock(fds);

  RollingBuffer buf;

  WriteAll(fds[1], "No newline");

  auto line = buf.ReadLine(fds[0]);
  CHECK(line.Empty());

  WriteAll(fds[1], " and still nothing");
  line = buf.ReadLine(fds[0]);
  CHECK(line.Empty());

  WriteAll(fds[1], " but then\nThere is light\n");
  close(fds[1]);

  line = buf.ReadLine(fds[0]);
  CHECK(line == "No newline and still nothing but then\n");

  line = buf.ReadLine(fds[0]);
  CHECK(line == "There is light\n");

  close(fds[0]);
}

TEST_CASE("RollingBuffer: nonblocking behavior") {
  int fds[2];
  MakePipe(fds);
  MakeNonBlock(fds);

  RollingBuffer buf;

  WriteAll(fds[1], "Final thing.\n");

  auto line = buf.ReadLine(fds[0]);
  CHECK(line == "Final thing.\n");

  line = buf.ReadLine(fds[0]);
  CHECK(line.Empty());

  close(fds[0]);
  close(fds[1]);
}

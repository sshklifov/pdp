#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "system/poll_table.h"

#include <unistd.h>

using namespace pdp;

TEST_CASE("PollTable: poll timeout with no events") {
  PollTable pt;

  int fds[2];
  REQUIRE(pipe(fds) == 0);

  pt.Register(fds[0]);

  bool ready = pt.Poll(Milliseconds{0});
  CHECK_FALSE(ready);
  CHECK(pt.GetEventsUnchecked(fds[0]) == 0);

  close(fds[0]);
  close(fds[1]);
}

TEST_CASE("PollTable: detects readable fd") {
  PollTable pt;

  int fds[2];
  REQUIRE(pipe(fds) == 0);

  pt.Register(fds[0]);

  const char msg[] = "x";
  REQUIRE(write(fds[1], msg, sizeof(msg)) == sizeof(msg));

  bool ready = pt.Poll(Milliseconds{10});
  CHECK(ready);

  CHECK(pt.GetEventsOrZero(505) == 0);

  int ev = pt.GetEventsUnchecked(fds[0]);
  CHECK((ev & POLLIN) != 0);
  CHECK(pt.HasInputEventsUnchecked(fds[0]));

  close(fds[0]);
  close(fds[1]);
}

TEST_CASE("PollTable: multiple fds, mixed order") {
  PollTable pt;

  int a[2], b[2];
  REQUIRE(pipe(a) == 0);
  REQUIRE(pipe(b) == 0);

  pt.Register(b[0]);
  pt.Register(a[0]);

  const char msg[] = "x";
  REQUIRE(write(a[1], msg, sizeof(msg)) == sizeof(msg));

  bool ready = pt.Poll(Milliseconds{10});
  CHECK(ready);

  CHECK((pt.GetEventsUnchecked(a[0]) & POLLIN) != 0);
  CHECK(pt.HasInputEventsUnchecked(a[0]));

  CHECK(pt.GetEventsUnchecked(b[0]) == 0);
  CHECK(!pt.HasInputEventsUnchecked(b[0]));

  char x[2];
  REQUIRE(read(a[0], &x, sizeof(x)) == sizeof(msg));

  REQUIRE(write(b[1], msg, sizeof(msg)) == sizeof(msg));

  ready = pt.Poll(Milliseconds{10});
  CHECK(ready);

  CHECK(pt.GetEventsUnchecked(a[0]) == 0);
  CHECK(!pt.HasInputEventsUnchecked(a[0]));

  CHECK((pt.GetEventsUnchecked(b[0]) & POLLIN) != 0);
  CHECK(pt.HasInputEventsUnchecked(b[0]));

  close(a[0]);
  close(a[1]);
  close(b[0]);
  close(b[1]);
}
TEST_CASE("PollTable: reset clears table") {
  PollTable pt;

  int fds[2];
  REQUIRE(pipe(fds) == 0);

  pt.Register(fds[0]);
  pt.Reset();

  // Re-register should work cleanly
  pt.Register(fds[0]);
  CHECK_FALSE(pt.Poll(Milliseconds{0}));
  CHECK(pt.GetEventsUnchecked(fds[0]) == 0);

  close(fds[0]);
  close(fds[1]);
}

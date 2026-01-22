#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "system/child_reaper.h"

namespace {

struct ReapRecord {
  pid_t pid = -1;
  int status = 0;
  int called = 0;
};

void OnReaped(void *ud, pid_t pid, int status) {
  auto *rec = static_cast<ReapRecord *>(ud);
  rec->pid = pid;
  rec->status = status;
  rec->called++;
}

pid_t ForkExit(int code) {
  pid_t pid = fork();
  REQUIRE(pid >= 0);

  if (pid == 0) {
    _exit(code);
  }
  return pid;
}

}  // namespace

TEST_CASE("ChildReaper: reap single child") {
  pdp::ChildReaper reaper;

  ReapRecord rec;
  pid_t pid = ForkExit(42);

  reaper.OnChildExited(&rec, pid, OnReaped);

  reaper.ReapAll();

  REQUIRE(rec.called == 1);
  CHECK(rec.pid == pid);
  CHECK(WIFEXITED(rec.status));
  CHECK(WEXITSTATUS(rec.status) == 42);
}

TEST_CASE("ChildReaper: reap multiple children") {
  pdp::ChildReaper reaper;

  ReapRecord r1, r2, r3;
  pid_t p1 = ForkExit(1);
  pid_t p2 = ForkExit(2);
  pid_t p3 = ForkExit(3);

  reaper.OnChildExited(&r1, p1, OnReaped);
  reaper.OnChildExited(&r2, p2, OnReaped);
  reaper.OnChildExited(&r3, p3, OnReaped);

  reaper.ReapAll();

  CHECK(r1.called == 1);
  CHECK(r2.called == 1);
  CHECK(r3.called == 1);

  CHECK(WEXITSTATUS(r1.status) == 1);
  CHECK(WEXITSTATUS(r2.status) == 2);
  CHECK(WEXITSTATUS(r3.status) == 3);
}

TEST_CASE("ChildReaper: reap with no children is safe") {
  pdp::ChildReaper reaper;

  // should not crash, block, or assert
  reaper.Reap();
  reaper.ReapAll();
}

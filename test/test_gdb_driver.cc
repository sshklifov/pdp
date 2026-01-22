#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "drivers/gdb_driver.h"

#include <sys/poll.h>
#include <unistd.h>

using namespace pdp;

namespace {

pdp::GdbRecordKind ReadWithTimeout(GdbDriver &gdb, GdbRecord *res, Milliseconds timeout) {
  struct pollfd poll_args;
  poll_args.fd = gdb.GetDescriptor();
  poll_args.events = POLLIN;
  poll_args.revents = 0;

  poll(&poll_args, 1, timeout.Get());
  return gdb.PollForRecords(res);
}

struct FakeGdb {
  int stdin_read;    // what GdbDriver writes to
  int stdout_write;  // what GdbDriver reads from
  int stderr_write;  // optinal: where GdbDriver monitors for errors

  ~FakeGdb() {
    close(stdin_read);
    close(stdout_write);
    close(stderr_write);
  }

  void WriteStdout(const char *s) { write(stdout_write, s, strlen(s)); }

  std::string ReadStdin() {
    char buf[256];
    ssize_t n = read(stdin_read, buf, sizeof(buf));
    REQUIRE(n > 0);
    return std::string(buf, n);
  }
};

FakeGdb SetupFakeGdb(GdbDriver &driver) {
  int in[2];
  int out[2];
  int err[2];

  pipe(in);
  pipe(out);
  pipe(err);

  driver.Start(in[1], out[0], err[0]);
  return {.stdin_read = in[0], .stdout_write = out[1], .stderr_write = err[1]};
}

}  // namespace

TEST_CASE("GdbDriver Send writes command to stdin") {
  GdbDriver driver;
  auto fake = SetupFakeGdb(driver);

  driver.Send(42, StringSlice("-exec-run"));

  std::string received = fake.ReadStdin();

  // MI commands MUST end with newline
  CHECK(received == "42-exec-run\n");
}

TEST_CASE("GdbDriver Poll parses result record") {
  GdbDriver driver;
  auto fake = SetupFakeGdb(driver);

  // Simulate GDB response
  fake.WriteStdout("42^done\n");

  GdbRecord rec;
  GdbRecordKind kind = ReadWithTimeout(driver, &rec, Milliseconds(10));

  CHECK(kind == GdbRecordKind::kResult);
  CHECK(rec.result_or_async.token == 42);
  CHECK(rec.result_or_async.kind == (uint32_t)GdbResultKind::kDone);
}

TEST_CASE("GdbDriver Poll parses async stopped event") {
  GdbDriver driver;
  auto fake = SetupFakeGdb(driver);

  fake.WriteStdout("*stopped,reason=\"breakpoint-hit\"\n");

  GdbRecord rec;
  GdbRecordKind kind = ReadWithTimeout(driver, &rec, Milliseconds(10));

  CHECK(kind == GdbRecordKind::kAsync);
  CHECK(rec.result_or_async.kind == (uint32_t)GdbAsyncKind::kStopped);
  CHECK(rec.result_or_async.results == StringSlice("reason=\"breakpoint-hit\""));
}

TEST_CASE("GdbDriver Poll times out cleanly") {
  GdbDriver driver;
  auto fake = SetupFakeGdb(driver);

  GdbRecord rec;
  GdbRecordKind kind = ReadWithTimeout(driver, &rec, Milliseconds(10));
  CHECK(kind == GdbRecordKind::kNone);
}

TEST_CASE("GdbDriver Poll parses stream output record") {
  GdbDriver driver;
  auto fake = SetupFakeGdb(driver);

  // Simulate GDB console stream output
  fake.WriteStdout("~\"hello from gdb\\n\"\n");

  GdbRecord rec;
  GdbRecordKind kind = ReadWithTimeout(driver, &rec, Milliseconds(10));

  CHECK(kind == GdbRecordKind::kStream);
  CHECK(rec.stream.message == StringSlice("hello from gdb\n"));
}

TEST_CASE("GdbDriver Poll maps unknown async event to kUnknown") {
  GdbDriver driver;
  auto fake = SetupFakeGdb(driver);

  fake.WriteStdout("=some-new-event,foo=\"bar\"\n");

  GdbRecord rec;
  GdbRecordKind kind = ReadWithTimeout(driver, &rec, Milliseconds(10));

  CHECK(kind == GdbRecordKind::kAsync);
  CHECK(rec.result_or_async.kind == (uint32_t)GdbAsyncKind::kUnknown);
  CHECK(rec.result_or_async.results == StringSlice("foo=\"bar\""));
}

TEST_CASE("ClassifyAsync basic async kinds") {
  CHECK(ClassifyAsync("stopped") == GdbAsyncKind::kStopped);
  CHECK(ClassifyAsync("running") == GdbAsyncKind::kRunning);
  CHECK(ClassifyAsync("cmd-param-changed") == GdbAsyncKind::kCmdParamChanged);
}

TEST_CASE("ClassifyAsync basic async kinds") {
  CHECK(ClassifyAsync("stopped") == GdbAsyncKind::kStopped);
  CHECK(ClassifyAsync("running") == GdbAsyncKind::kRunning);
  CHECK(ClassifyAsync("cmd-param-changed") == GdbAsyncKind::kCmdParamChanged);
}

TEST_CASE("ClassifyAsync thread events") {
  CHECK(ClassifyAsync("thread-created") == GdbAsyncKind::kThreadCreated);
  CHECK(ClassifyAsync("thread-selected") == GdbAsyncKind::kThreadSelected);
  CHECK(ClassifyAsync("thread-exited") == GdbAsyncKind::kThreadExited);
  CHECK(ClassifyAsync("thread-group-started") == GdbAsyncKind::kThreadGroupStarted);
}

TEST_CASE("ClassifyAsync library events") {
  CHECK(ClassifyAsync("library-loaded") == GdbAsyncKind::kLibraryLoaded);
  CHECK(ClassifyAsync("library-unloaded") == GdbAsyncKind::kLibraryUnloaded);
}

TEST_CASE("ClassifyResult known results") {
  CHECK(ClassifyResult("done") == GdbResultKind::kDone);
  CHECK(ClassifyResult("running") == GdbResultKind::kDone);
  CHECK(ClassifyResult("error") == GdbResultKind::kError);
  CHECK(ClassifyResult("exit") == GdbResultKind::kError);
}

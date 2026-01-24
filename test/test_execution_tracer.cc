#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "tracing/execution_tracer.h"

#include <fcntl.h>
#include <unistd.h>
#include <cstring>

using namespace pdp;

TEST_CASE("ExecutionTracer: record and replay simple read") {
  const char *input_path = "/tmp/pdp_input.txt";
  const char *record_path = "/tmp/pdp_record.bin";

  unlink(input_path);
  unlink(record_path);

  // ------------------------------------------------------------
  // Prepare input file
  // ------------------------------------------------------------
  {
    int fd = open(input_path, O_CREAT | O_WRONLY | O_TRUNC, 0644);
    REQUIRE(fd >= 0);

    const char payload[] = "hello record replay";
    ssize_t w = write(fd, payload, sizeof(payload));
    REQUIRE(w == sizeof(payload));

    close(fd);
  }

  // ------------------------------------------------------------
  // RECORD
  // ------------------------------------------------------------
  auto &tracer = g_recorder;

  tracer.StartRecording(record_path);

  int fd = open(input_path, O_RDONLY);
  REQUIRE(fd >= 0);

  char record_buf[64] = {};
  ssize_t record_ret = tracer.SyscallRead(fd, record_buf, sizeof(record_buf));

  CHECK(record_ret > 0);
  CHECK(std::memcmp(record_buf, "hello record replay", record_ret) == 0);

  close(fd);
  tracer.StopRecording();

  // ------------------------------------------------------------
  // REPLAY
  // ------------------------------------------------------------
  tracer.StartReplaying(record_path);

  fd = open(input_path, O_RDONLY);
  REQUIRE(fd >= 0);

  char replay_buf[64] = {};
  ssize_t replay_ret = tracer.SyscallRead(fd, replay_buf, sizeof(replay_buf));

  CHECK(replay_ret == record_ret);
  CHECK(std::memcmp(replay_buf, record_buf, replay_ret) == 0);

  close(fd);
  tracer.StopReplaying();
}

TEST_CASE("ExecutionTracer: record and replay poll") {
  const char *record_path = "/tmp/pdp_poll_record.bin";
  unlink(record_path);

  int pipefd[2];
  REQUIRE(pipe(pipefd) == 0);

  ExecutionTracer &tracer = g_recorder;

  // ------------------------------------------------------------
  // RECORD
  // ------------------------------------------------------------
  tracer.StartRecording(record_path);

  // Make pipe readable
  const char byte = 'x';
  REQUIRE(write(pipefd[1], &byte, 1) == 1);

  struct pollfd pfd{};
  pfd.fd = pipefd[0];
  pfd.events = POLLIN;
  pfd.revents = 0;

  int record_ret = tracer.SyscallPoll(&pfd, 1, 0);
  CHECK(record_ret == 1);
  bool has_data = pfd.revents & POLLIN;
  CHECK(has_data);

  tracer.StopRecording();

  // ------------------------------------------------------------
  // REPLAY
  // ------------------------------------------------------------
  tracer.StartReplaying(record_path);

  struct pollfd replay_pfd{};
  replay_pfd.fd = pipefd[0];
  replay_pfd.events = POLLIN;
  replay_pfd.revents = 0;

  int replay_ret = tracer.SyscallPoll(&replay_pfd, 1, 0);
  CHECK(replay_ret == 1);
  bool has_replay_data = replay_pfd.revents & POLLIN;
  CHECK(has_replay_data);

  tracer.StopReplaying();

  close(pipefd[0]);
  close(pipefd[1]);
}

TEST_CASE("ExecutionTracer: record and replay fork") {
  const char *record_path = "/tmp/pdp_fork_record.bin";
  unlink(record_path);

  ExecutionTracer &tracer = g_recorder;

  pid_t recorded_pid = -1;
  int recorded_status = 0;

  // ------------------------------------------------------------
  // RECORD
  // ------------------------------------------------------------
  tracer.StartRecording(record_path);

  pid_t pid = tracer.SyscallFork();
  REQUIRE(pid >= 0);

  if (pid == 0) {
    // Child
    _exit(42);
  }

  // Parent
  recorded_pid = pid;

  pid_t waited = tracer.SyscallWaitPid(&recorded_status, 0);
  CHECK(waited == recorded_pid);
  CHECK(WIFEXITED(recorded_status));
  CHECK(WEXITSTATUS(recorded_status) == 42);

  tracer.StopRecording();

  // ------------------------------------------------------------
  // REPLAY
  // ------------------------------------------------------------
  tracer.StartReplaying(record_path);

  pid_t replay_pid = tracer.SyscallFork();
  CHECK(replay_pid == recorded_pid);

  int replay_status = 0;
  pid_t replay_waited = tracer.SyscallWaitPid(&replay_status, 0);

  CHECK(replay_waited == recorded_pid);
  CHECK(replay_status == recorded_status);

  tracer.StopReplaying();
}

TEST_CASE("ExecutionTracer: read error then success (single call path)") {
  const char *record_path = "/tmp/pdp_read_err_then_ok.bin";
  unlink(record_path);

  ExecutionTracer &tracer = g_recorder;

  int bad_fd = -1;
  int good_fd = -1;
  ssize_t err_ret = 0;
  ssize_t ok_ret = 0;

  char ok_buf[64] = {};
  const char payload[] = "read after error works";

  // ------------------------------------------------------------
  // RECORD
  // ------------------------------------------------------------
  tracer.StartRecording(record_path);

  // 1) Error read
  {
    int pipefd[2];
    REQUIRE(pipe(pipefd) == 0);

    close(pipefd[0]);    // invalidate read end
    bad_fd = pipefd[0];  // store exact fd value

    char tmp[16];
    err_ret = tracer.SyscallRead(bad_fd, tmp, sizeof(tmp));
    CHECK(err_ret == -1);

    close(pipefd[1]);
  }

  // 2) Successful read
  {
    const char *input_path = "/tmp/pdp_read_ok2.txt";
    unlink(input_path);

    int fd = open(input_path, O_CREAT | O_WRONLY | O_TRUNC, 0644);
    REQUIRE(fd >= 0);
    write(fd, payload, sizeof(payload));
    close(fd);

    good_fd = open(input_path, O_RDONLY);
    REQUIRE(good_fd >= 0);

    ok_ret = tracer.SyscallRead(good_fd, ok_buf, sizeof(ok_buf));
    CHECK(ok_ret == sizeof(payload));
    CHECK(memcmp(ok_buf, payload, ok_ret) == 0);

    close(good_fd);
  }

  tracer.StopRecording();

  // ------------------------------------------------------------
  // REPLAY
  // ------------------------------------------------------------
  tracer.StartReplaying(record_path);

  // Same calls, same order, same arguments

  {
    char tmp[16];
    ssize_t r = tracer.SyscallRead(bad_fd, tmp, sizeof(tmp));
    CHECK(r == err_ret);
  }

  {
    char replay_buf[64] = {};
    ssize_t r = tracer.SyscallRead(good_fd, replay_buf, sizeof(replay_buf));
    CHECK(r == ok_ret);
    CHECK(memcmp(replay_buf, payload, r) == 0);
  }

  tracer.StopReplaying();
}

TEST_CASE("ExecutionTracer: waitpid error then fork+waitpid success (record/replay)") {
  const char *record_path = "/tmp/pdp_waitpid_err_then_ok.bin";
  unlink(record_path);

  using namespace pdp;
  ExecutionTracer &tracer = g_recorder;

  pid_t rec_err_pid = 0;
  pid_t rec_fork_pid = 0;
  pid_t rec_ok_pid = 0;
  int rec_ok_status = 0;

  pid_t rep_err_pid = 0;
  pid_t rep_fork_pid = 0;
  pid_t rep_ok_pid = 0;
  int rep_ok_status = 0;

  // ------------------------------------------------------------
  // RECORD
  // ------------------------------------------------------------
  tracer.StartRecording(record_path);
  {
    int status = 0;

    // 1) waitpid with no children → should fail
    rec_err_pid = tracer.SyscallWaitPid(&status, 0);
    CHECK(rec_err_pid == -1);

    // 2) fork
    rec_fork_pid = tracer.SyscallFork();
    REQUIRE(rec_fork_pid >= 0);

    if (rec_fork_pid == 0) {
      _exit(37);
    }

    // 3) successful waitpid
    rec_ok_pid = tracer.SyscallWaitPid(&rec_ok_status, 0);
    CHECK(rec_ok_pid == rec_fork_pid);
    CHECK(WIFEXITED(rec_ok_status));
    CHECK(WEXITSTATUS(rec_ok_status) == 37);
  }
  tracer.StopRecording();

  // ------------------------------------------------------------
  // REPLAY
  // ------------------------------------------------------------
  tracer.StartReplaying(record_path);
  {
    int status = 0;

    // Same calls, same order, same arguments.
    rep_err_pid = tracer.SyscallWaitPid(&status, 0);
    CHECK(rep_err_pid == rec_err_pid);

    rep_fork_pid = tracer.SyscallFork();
    CHECK(rep_fork_pid == rec_fork_pid);

    rep_ok_pid = tracer.SyscallWaitPid(&rep_ok_status, 0);
    CHECK(rep_ok_pid == rec_ok_pid);
    CHECK(rep_ok_status == rec_ok_status);
  }
  tracer.StopReplaying();
}

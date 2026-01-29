#pragma once

#include "core/monotonic_check.h"
#include "core/once_guard.h"
#include "data/unique_ptr.h"
#include "strings/rolling_buffer.h"
#include "system/child_reaper.h"
#include "system/file_descriptor.h"
#include "tracing/execution_tracer.h"

#include <fcntl.h>
#include <sys/prctl.h>
#include <unistd.h>
#include <csignal>

namespace pdp {

enum class GdbAsyncKind {
  kStopped,
  kRunning,
  kCmdParamChanged,
  kBreakpointCreated,
  kBreakpointDeleted,
  kBreakpointModified,
  kThreadCreated,
  kThreadSelected,
  kThreadExited,
  kThreadGroupStarted,
  kLibraryLoaded,
  kLibraryUnloaded,
  kUnknown
};

enum class GdbResultKind { kDone, kError, kUnknown };

enum class GdbRecordKind { kStream, kAsync, kResult, kNone };

union GdbRecord {
  GdbRecord() {}

  GdbRecordKind SetStream(const StringSlice &msg);
  GdbRecordKind SetAsync(GdbAsyncKind kind, const StringSlice &results);
  GdbRecordKind SetResult(uint32_t token, GdbResultKind kind, const StringSlice &results);

  struct GdbStream {
    StringSlice message;
  } stream;

  struct GdbResultOrAsync {
    uint32_t token;
    uint32_t kind;
    StringSlice results;
  } result_or_async;
};

static_assert(sizeof(GdbRecord) == 24);
static_assert(std::is_trivially_destructible_v<GdbRecord>);

GdbAsyncKind ClassifyAsync(StringSlice name);
GdbResultKind ClassifyResult(StringSlice name);

struct GdbDriver {
  GdbDriver();
  ~GdbDriver();

  template <typename... Args>
  void Start(ChildReaper &r, const char *path, Args... argv) {
    static_assert((std::is_same_v<Args, const char *> && ...),
                  "All exec arguments must be exactly const char*");

    int in[2], out[2], err[2];
    CheckFatal(pipe2(in, O_CLOEXEC), "GDB pipe(in)");
    CheckFatal(pipe2(out, O_CLOEXEC), "GDB pipe(out)");
    CheckFatal(pipe2(err, O_CLOEXEC), "GDB pipe(err)");

    pid_t pid = g_recorder.SyscallFork();
    CheckFatal(pid, "GDB fork");
    if (pid == 0) {
      // child: GDB
      prctl(PR_SET_PDEATHSIG, SIGTERM);

      dup2(in[0], STDIN_FILENO);
      dup2(out[1], STDOUT_FILENO);
      dup2(err[1], STDERR_FILENO);

      close(in[1]);
      close(out[0]);
      close(err[0]);

      execl(path, path, argv..., (char *)0);
      _exit(127);
    }

    // parent
    close(in[0]);
    close(out[1]);
    close(err[1]);
    r.WatchChild(pid, GdbDriver::OnGdbExited, this);
    gdb_pid = pid;
    Start(in[1], out[0], err[0]);
  }

  void Start(ChildReaper &reaper) {
    Start(reaper, "/usr/bin/gdb", "--quiet", "-iex", "set pagination off", "-iex", "set prompt",
          "-iex", "set startup-with-shell off", "--interpreter=mi2", "Debug/pdp");
  }

  void Start(int input_fd, int output_fd, int error_fd) {
    started_once.Set();
    gdb_stdin.SetDescriptor(input_fd);
    gdb_stdout.SetDescriptor(output_fd);
    gdb_stderr.SetDescriptor(error_fd);
  }

  template <typename... Args>
  void Send(uint32_t token, const StringSlice &fmt, Args &&...args) {
    auto packed_args = MakePackedArgs(std::forward<Args>(args)...);
    return Send(token, fmt, packed_args.slots, packed_args.type_bits);
  }

  void Send(uint32_t token, const StringSlice &fmt, PackedValue *args, uint64_t type_bits);

  int GetDescriptor() const;
  int GetErrorDescriptor() const;

  GdbRecordKind PollForRecords(GdbRecord *res);
  StringSlice PollForErrors();

 private:
  static void MonitorGdbStderr(std::atomic_bool *is_running, int fd);
  static void OnGdbExited(pid_t pid, int status, void *user_data) {
    PDP_IGNORE(pid);
    PDP_IGNORE(user_data);
    ChildReaper::PrintStatus("Gdb", status);

    // TODO: Stop execution!
  }

  OnceGuard started_once;
  MonotonicCheck token_checker;

  pid_t gdb_pid;

  RollingBuffer gdb_stdout;
  OutputDescriptor gdb_stdin;
  InputDescriptor gdb_stderr;

  StringBuffer error_buffer;
  static constexpr size_t max_error_length = 256;
};

};  // namespace pdp

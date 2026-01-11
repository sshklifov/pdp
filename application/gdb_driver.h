#pragma once

#include "strings/rolling_buffer.h"
#include "system/file_descriptor.h"
#include "system/thread.h"
#include "system/time_units.h"

namespace pdp {

enum class AsyncKind {
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

enum class ResultKind { kDone, kError, kUnknown };

enum class RecordKind { kStream, kAsync, kResult, kNone };

union GdbRecord {
  GdbRecord() {}

  RecordKind SetStream(const StringSlice &msg);
  RecordKind SetAsync(AsyncKind kind, const StringSlice &results);
  RecordKind SetResult(ResultKind kind, const StringSlice &results);

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

struct GdbDriver {
  GdbDriver();

  ~GdbDriver();

  void Start();

  template <typename T, typename... Args>
  bool Send(uint32_t token, const StringSlice &fmt, Args &&...args) {
    auto packed_args = MakePackedArgs(std::forward<Args>(args)...);
    return Send(token, fmt, packed_args.slots, packed_args.type_bits);
  }

  bool Send(uint32_t token, const StringSlice &fmt, PackedValue *args, uint64_t type_bits);

  bool Send(uint32_t token, const StringSlice &msg);

  RecordKind Poll(Milliseconds timeout, GdbRecord *res);

 private:
  static void MonitorGdbStderr(std::atomic_bool *is_running, int fd);

  OutputDescriptor gdb_stdin;
  RollingBuffer gdb_stdout;
  StoppableThread monitor_thread;
#ifdef PDP_ENABLE_ASSERT
  uint32_t last_token;
#endif
};

};  // namespace pdp

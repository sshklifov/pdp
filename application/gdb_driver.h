#pragma once

#include "data/callback_table.h"
#include "data/scoped_ptr.h"
#include "parser/expr.h"
#include "strings/rolling_buffer.h"
#include "system/file_descriptor.h"
#include "system/thread.h"
#include "system/time_units.h"

namespace pdp {

inline bool IsStreamMessage(const StringSlice &s) {
  return s[0] == '~' || s[0] == '@' || s[0] == '&';
}

inline bool IsAsyncMarker(char c) { return c == '*' || c == '+' || c == '='; }

inline bool IsResultMarker(char c) { return c == '^'; }

// TODO?
using Session = void *;

struct GdbDriver {
  GdbDriver();

  void Start();

  template <typename Callable, typename... Args>
  void Request(const StringSlice &command, Args &&...args) {
    uint32_t request_token = token_counter;
    // XXX: Race condition if class used from multiple threads.
    if (Request(command)) {
      callbacks.Bind<Callable>(request_token, std::forward<Args>(args)...);
    }
  }

  bool Request(const StringSlice &command);

  void Poll(Milliseconds timeout);

 private:
  static void MonitorGdbStderr(std::atomic_bool *is_running, int fd);

  void OnStreamMessage(const StringSlice &message);
  void OnAsyncMessage(const StringSlice &class_name, ScopedPtr<ExprBase> &&expr);
  void OnResultMessage(uint32_t token, const StringSlice &class_name, ScopedPtr<ExprBase> &&expr);

  OutputDescriptor gdb_stdin;
  RollingBuffer gdb_stdout;
  StoppableThread monitor_thread;

  CallbackTable<Session> callbacks;
  unsigned token_counter;
};

};  // namespace pdp

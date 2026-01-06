#include "gdb_driver.h"

#include "core/check.h"
#include "parser/mi_parser.h"
#include "tracing/trace_likely.h"

#include <unistd.h>

namespace pdp {

StringSlice ProcessCstringInPlace(char *begin, char *end) {
  if (PDP_UNLIKELY(end - begin < 2 || begin[0] != '\"' || end[-1] != '\"')) {
    pdp_error("Unexpected start/end of stream message");
    return StringSlice(begin, end);
  }
  ++begin;
  --end;

  // Skip to first copy required.
  char *write_head = static_cast<char *>(memchr(begin, '\\', end - begin));
  if (!write_head) {
    return StringSlice(begin, end);
  }

  char *read_head = write_head;
  while (read_head < end) {
    if (PDP_LIKELY(*read_head != '\\')) {
      *write_head = *read_head;
      ++write_head;
      ++read_head;
    } else {
      *write_head = ReverseEscapeCharacter(read_head[1]);
      write_head += 1;
      read_head += 2;
    }
  }
  return StringSlice(begin, write_head);
}

AsyncKind ClassifyAsync(StringSlice name) {
  StringSlice prefix;
  switch (name[0]) {
    case 's':
      if (PDP_LIKELY(name == "stopped")) {
        return AsyncKind::kStopped;
      }
      break;
    case 'r':
      if (PDP_LIKELY(name == "running")) {
        return AsyncKind::kRunning;
      }
    case 'c':
      if (PDP_LIKELY(name == "cmd-param-changed")) {
        return AsyncKind::kCmdParamChanged;
      }
    case 'b':
      prefix = "breakpoint-";
      if (PDP_LIKELY(name.Size() >= prefix.Size() + 1 && name.MemCmp(prefix) == 0)) {
        name.DropLeft(prefix.Size());
        switch (name[0]) {
          case 'c':
            if (PDP_LIKELY(name == "created")) {
              return AsyncKind::kBreakpointCreated;
            }
            break;
          case 'd':
            if (PDP_LIKELY(name == "deleted")) {
              return AsyncKind::kBreakpointDeleted;
            }
            break;
          case 'm':
            if (PDP_LIKELY(name == "modified")) {
              return AsyncKind::kBreakpointModified;
            }
            break;
        }
      }
      break;
    case 't':
      prefix = "thread-";
      if (PDP_LIKELY(name.Size() >= prefix.Size() + 1 && name.MemCmp(prefix) == 0)) {
        name.DropLeft(prefix.Size());
        switch (name[7]) {
          case 'c':
            if (PDP_LIKELY(name == "created")) {
              return AsyncKind::kThreadCreated;
            }
            break;
          case 's':
            if (PDP_LIKELY(name == "selected")) {
              return AsyncKind::kThreadSelected;
            }
            break;
          case 'e':
            if (PDP_LIKELY(name == "exitted")) {
              return AsyncKind::kThreadExited;
            }
          case 'g':
            if (name == "group-started") {
              return AsyncKind::kThreadGroupStarted;
            }
            break;
        }
      }
      break;
    case 'l':
      prefix = "library-";
      if (PDP_LIKELY(name.Size() >= prefix.Size() + 1 && name.MemCmp(prefix) == 0)) {
        name.DropLeft(prefix.Size());
        if (PDP_LIKELY(name == "loaded")) {
          return AsyncKind::kLibraryLoaded;
        } else if (PDP_LIKELY(name == "unloaded")) {
          return AsyncKind::kLibraryLoaded;
        }
      }
      break;
  }
  return AsyncKind::kUnknown;
}

GdbDriver::GdbDriver() : token_counter(1) {}

GdbDriver::~GdbDriver() { monitor_thread.Stop(); }

void GdbDriver::Start() {
  int in[2], out[2], err[2];
  pipe(in);
  pipe(out);
  pipe(err);

  pid_t pid = fork();
  if (pid == 0) {
    // child: GDB
    dup2(in[0], STDIN_FILENO);
    dup2(out[1], STDOUT_FILENO);
    dup2(err[1], STDERR_FILENO);

    close(in[1]);
    close(out[0]);
    close(err[0]);

    int ret =
        execlp("gdb", "gdb", "--quiet", "-iex", "set pagination off", "-iex", "set prompt", "-iex",
               "set startup-with-shell off", "--interpreter=mi2", "Debug/pdp", nullptr);
    Check(ret, "execlp");
    _exit(1);
  }

  // parent
  close(in[0]);
  close(out[1]);
  close(err[1]);

  gdb_stdin.SetValue(in[1]);
  gdb_stdout.SetDescriptor(out[0]);
  monitor_thread.Start(MonitorGdbStderr, err[0]);
}

void GdbDriver::MonitorGdbStderr(std::atomic_bool *is_running, int fd) {
  InputDescriptor gdb_stderr(fd);
  DefaultAllocator allocator;

  while (*is_running) {
    bool has_data = gdb_stderr.WaitForInput(Milliseconds(1000));
    while (has_data) {
      const size_t max_print = 1024;
      char *buf = Allocate<char>(allocator, max_print);
      size_t n = gdb_stderr.ReadAvailable(buf, max_print);
      if (PDP_LIKELY(n > 0)) {
        pdp_error("GDB error: {}", StringSlice(buf, n));
      }
      Deallocate<char>(allocator, buf);
    }
  }
}

void GdbDriver::Poll(Milliseconds timeout) {
  MutableLine line = gdb_stdout.ReadLine(timeout);
  size_t length = line.end - line.begin;
  if (PDP_TRACE_LIKELY(length <= 1)) {
    return;
  }
  pdp_assert(line.begin[length - 1] == '\n');

  if (IsStreamMarker(*line.begin)) {
    OnStreamMessage(ProcessCstringInPlace(line.begin + 1, line.end - 1));
  } else {
    const char *it = line.begin;
    uint32_t token = 0;
    while (*it >= '0' && *it <= '9') {
      token *= 10;
      token += (*it - '0');
      ++it;
    }

    char marker = *it;
    ++it;

    const char *name_begin = it;
    while (*it != '\n' && *it != ',') {
      ++it;
    }
    StringSlice name(name_begin, it);
    StringSlice record(it + 1, line.end);
    if (!record.Empty()) {
      record.DropRight(1);
    }
    MiFirstPass first_pass(record);
    if (PDP_UNLIKELY(!first_pass.Parse())) {
      pdp_error("Parsing {} failed!", record);
      return;
    }
    MiSecondPass second_pass(record, first_pass);
    ScopedPtr<ExprBase> expr(second_pass.Parse());
    if (PDP_UNLIKELY(!expr)) {
      pdp_error("Parsing {} failed!", record);
      return;
    }

    if (PDP_UNLIKELY(name.Empty())) {
      pdp_warning("Missing class name for message with token {}", token);
    } else if (IsResultMarker(marker)) {
      OnResultMessage(token, name, std::move(expr));
    } else if (PDP_LIKELY(IsAsyncMarker(marker))) {
      OnAsyncMessage(name, std::move(expr));
    }
  }
}

bool GdbDriver::Request(const StringSlice &command) {
  StringBuilder builder;
  builder.Append(token_counter);
  builder.Append(command);
  builder.Append('\n');

  ++token_counter;

  bool success = gdb_stdin.WriteExactly(builder.Data(), builder.Size(), Milliseconds(1000));
  if (PDP_UNLIKELY(!success)) {
    pdp_warning("Failed to submit request {}", command);
  }
  return success;
}

void GdbDriver::OnStreamMessage(const StringSlice &message) { LogUnformatted(message); }

// TODO first pass -> second pass is creating a lot of empty objects. bad?

void GdbDriver::OnAsyncMessage(const StringSlice &name, ScopedPtr<ExprBase> expr) {
  auto async_class = ClassifyAsync(name);
}

void GdbDriver::OnResultMessage(uint32_t token, const StringSlice &name,
                                ScopedPtr<ExprBase> expr) {
  if (PDP_LIKELY(name == "done")) {
    // callbacks.Invoke(token, ...);
  } else if (PDP_LIKELY(name == "error")) {
    // TODO
  }
}

};  // namespace pdp

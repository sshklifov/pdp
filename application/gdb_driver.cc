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

GdbDriver::GdbDriver() : token_counter(1) {}

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
  StringSlice str = gdb_stdout.ReadLine(timeout);
  if (PDP_TRACE_LIKELY(str.Size() <= 1)) {
    return;
  }
  pdp_assert(str[str.Size() - 1] == '\n');

  if (IsStreamMessage(str)) {
    // TODO const_cast
    OnStreamMessage(ProcessCstringInPlace(const_cast<char *>(str.Begin() + 1),
                                          const_cast<char *>(str.End() - 1)));
  } else {
    const char *it = str.Begin();
    uint32_t token = 0;
    while (*it >= '0' && *it <= '9') {
      token *= 10;
      token += (*it - '0');
      ++it;
    }

    char marker = *it;
    ++it;

    const char *class_begin = it;
    while (*it != '\n' && *it != ',') {
      ++it;
    }
    StringSlice class_name(class_begin, it);
    StringSlice record(it + 1, str.End());
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

    if (PDP_UNLIKELY(class_name.Empty())) {
      pdp_warning("Missing class name for message with token {}", token);
    } else if (IsResultMarker(marker)) {
      OnResultMessage(token, class_name, std::move(expr));
    } else if (PDP_LIKELY(IsAsyncMarker(marker))) {
      OnAsyncMessage(class_name, std::move(expr));
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

void GdbDriver::OnAsyncMessage(const StringSlice &class_name, ScopedPtr<ExprBase> &&expr) {
  switch (class_name[0]) {
    case 's':
      if (PDP_LIKELY(class_name == "stopped")) {
        // TODO
      }
      break;
    case 'r':
      if (PDP_LIKELY(class_name == "running")) {
        // TODO
      }
    case 'c':
      if (PDP_LIKELY(class_name == "cmd-param-changed")) {
        // TODO
      }
    case 'b':
      if (PDP_LIKELY(class_name.Size() >= 12 && class_name.MemCmp("breakpoint-") == 0)) {
        switch (class_name[11]) {
          case 'c':
            if (PDP_LIKELY(class_name.Substr(class_name.Begin() + 11) == "created")) {
              // TODO
            }
            break;
          case 'd':
            if (PDP_LIKELY(class_name.Substr(class_name.Begin() + 11) == "deleted")) {
              // TODO
            }
            break;
          case 'm':
            if (PDP_LIKELY(class_name.Substr(class_name.Begin() + 11) == "modified")) {
              // TODO
            }
            break;
        }
      }
      break;
    case 't':
      if (PDP_LIKELY(class_name.Size() >= 8 && class_name.MemCmp("thread-") == 0)) {
        switch (class_name[7]) {
          case 'c':
            if (PDP_LIKELY(class_name.Substr(class_name.Begin() + 7) == "created")) {
              // TODO
            }
            break;
          case 's':
            if (PDP_LIKELY(class_name.Substr(class_name.Begin() + 7) == "selected")) {
              // TODO
            }
            break;
          case 'e':
            if (PDP_LIKELY(class_name.Substr(class_name.Begin() + 7) == "exitted")) {
              // TODO
            }
          case 'g':
            if (class_name.Substr(class_name.Begin() + 7) == "group-started") {
              // TODO
            }
            break;
        }
      }
      break;
    case 'l':
      if (PDP_LIKELY(class_name.Size() >= 9 && class_name.MemCmp("library-") == 0)) {
        if (PDP_LIKELY(class_name.Substr(class_name.Begin() + 8) == "loaded")) {
          // TODO
        } else if (PDP_LIKELY(class_name.Substr(class_name.Begin() + 8) == "unloaded")) {
          // TODO
        }
      }
      break;
  }
}

void GdbDriver::OnResultMessage(uint32_t token, const StringSlice &class_name,
                                ScopedPtr<ExprBase> &&expr) {
  if (PDP_LIKELY(class_name == "done")) {
    // callbacks.Invoke(token, ...);
  } else if (PDP_LIKELY(class_name == "error")) {
    // TODO
  }
}

};  // namespace pdp

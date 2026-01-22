#include "gdb_driver.h"

#include "core/check.h"
#include "core/log.h"
#include "parser/mi_parser.h"

#include <unistd.h>

namespace pdp {

inline bool IsStreamMarker(char c) { return c == '~' || c == '@' || c == '&'; }

inline bool IsAsyncMarker(char c) { return c == '*' || c == '+' || c == '='; }

inline bool IsResultMarker(char c) { return c == '^'; }

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

GdbAsyncKind ClassifyAsync(StringSlice name) {
  StringSlice prefix;
  switch (name[0]) {
    case 's':
      if (PDP_LIKELY(name == "stopped")) {
        return GdbAsyncKind::kStopped;
      }
      break;
    case 'r':
      if (PDP_LIKELY(name == "running")) {
        return GdbAsyncKind::kRunning;
      }
      break;
    case 'c':
      if (PDP_LIKELY(name == "cmd-param-changed")) {
        return GdbAsyncKind::kCmdParamChanged;
      }
      break;
    case 'b':
      prefix = "breakpoint-";
      if (PDP_LIKELY(name.Size() >= prefix.Size() + 1 && name.MemCmp(prefix) == 0)) {
        name.DropLeft(prefix.Size());
        switch (name[0]) {
          case 'c':
            if (PDP_LIKELY(name == "created")) {
              return GdbAsyncKind::kBreakpointCreated;
            }
            break;
          case 'd':
            if (PDP_LIKELY(name == "deleted")) {
              return GdbAsyncKind::kBreakpointDeleted;
            }
            break;
          case 'm':
            if (PDP_LIKELY(name == "modified")) {
              return GdbAsyncKind::kBreakpointModified;
            }
            break;
        }
      }
      break;
    case 't':
      prefix = "thread-";
      if (PDP_LIKELY(name.Size() >= prefix.Size() + 1 && name.MemCmp(prefix) == 0)) {
        name.DropLeft(prefix.Size());
        switch (name[0]) {
          case 'c':
            if (PDP_LIKELY(name == "created")) {
              return GdbAsyncKind::kThreadCreated;
            }
            break;
          case 's':
            if (PDP_LIKELY(name == "selected")) {
              return GdbAsyncKind::kThreadSelected;
            }
            break;
          case 'e':
            if (PDP_LIKELY(name == "exited")) {
              return GdbAsyncKind::kThreadExited;
            }
            break;
          case 'g':
            if (name == "group-started") {
              return GdbAsyncKind::kThreadGroupStarted;
            }
            break;
        }
      }
      break;
    case 'l':
      prefix = "library-";
      if (PDP_LIKELY(name.Size() >= prefix.Size() && name.MemCmp(prefix) == 0)) {
        name.DropLeft(prefix.Size());
        if (PDP_LIKELY(name == "loaded")) {
          return GdbAsyncKind::kLibraryLoaded;
        } else if (PDP_LIKELY(name == "unloaded")) {
          return GdbAsyncKind::kLibraryUnloaded;
        }
      }
      break;
  }
  return GdbAsyncKind::kUnknown;
}

GdbResultKind ClassifyResult(StringSlice name) {
  switch (name[0]) {
    case 'd':
      if (PDP_LIKELY(name == "done")) {
        return GdbResultKind::kDone;
      }
      break;
    case 'r':
      if (PDP_LIKELY(name == "running")) {
        return GdbResultKind::kDone;
      }
      break;
    case 'e':
      if (PDP_LIKELY(name == "error" || name == "exit")) {
        return GdbResultKind::kError;
      }
      break;
  }
  return GdbResultKind::kUnknown;
}

GdbRecordKind GdbRecord::SetStream(const StringSlice &msg) {
  stream.message = msg;
  return GdbRecordKind::kStream;
}

GdbRecordKind GdbRecord::SetAsync(GdbAsyncKind kind, const StringSlice &results) {
  result_or_async.token = 0;
  result_or_async.kind = static_cast<uint32_t>(kind);
  result_or_async.results = results;
  return GdbRecordKind::kAsync;
}

GdbRecordKind GdbRecord::SetResult(uint32_t token, GdbResultKind kind, const StringSlice &results) {
  result_or_async.token = token;
  result_or_async.kind = static_cast<uint32_t>(kind);
  result_or_async.results = results;
  return GdbRecordKind::kResult;
}

GdbDriver::GdbDriver() : error_buffer(max_error_length) {}

GdbRecordKind GdbDriver::PollForRecords(GdbRecord *res) {
  MutableLine line = gdb_stdout.ReadLine();
  size_t length = line.end - line.begin;
  if (PDP_LIKELY(length <= 1)) {
    return GdbRecordKind::kNone;
  }
  pdp_assert(line.begin[length - 1] == '\n');

  if (IsStreamMarker(*line.begin)) {
    return res->SetStream(ProcessCstringInPlace(line.begin + 1, line.end - 1));
  }

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
  const char *name_end = it;

  StringSlice results(it + 1, line.end);
  if (!results.Empty()) {
    results.DropRight(1);
  }

  if (PDP_UNLIKELY(name_begin == name_end)) {
    pdp_warning("Missing class name for message with token {}", token);
  } else if (IsResultMarker(marker)) {
    GdbResultKind kind = ClassifyResult(StringSlice(name_begin, name_end));
    return res->SetResult(token, kind, results);
  } else if (PDP_LIKELY(IsAsyncMarker(marker))) {
    GdbAsyncKind kind = ClassifyAsync(StringSlice(name_begin, name_end));
    return res->SetAsync(kind, results);
  }
  return GdbRecordKind::kNone;
}

void GdbDriver::PollForErrors() {
  for (;;) {
    size_t n = gdb_stderr.ReadAvailable(error_buffer.Get(), max_error_length);
    if (PDP_LIKELY(n > 0)) {
      pdp_error("Gdb error");
      pdp_error_multiline(StringSlice(error_buffer.Get(), n));
    } else {
      return;
    }
  }
}

int GdbDriver::GetDescriptor() const { return gdb_stdout.GetDescriptor(); }

int GdbDriver::GetErrorDescriptor() const { return gdb_stderr.GetDescriptor(); }

void GdbDriver::Send(uint32_t token, const StringSlice &fmt, PackedValue *args,
                     uint64_t type_bits) {
  token_checker.Set(token);

  StringBuilder builder;
  builder.Append(token);
  builder.AppendPack(fmt, args, type_bits);
  builder.Append('\n');

  bool success = gdb_stdin.WriteExactly(builder.Data(), builder.Size(), Milliseconds(1000));
  if (PDP_UNLIKELY(!success)) {
    pdp_warning("Failed to submit request {}", builder.GetSlice());
  }
}

void GdbDriver::OnGdbExited(int status) {
  if (WIFSIGNALED(status)) {
    int sig = WTERMSIG(status);
    pdp_error("Gdb terminated by signal {}", GetSignalDescription(sig));
  } else if (WIFEXITED(status)) {
    int exit_status = WEXITSTATUS(status);
    pdp_warning("Gdb exited normally with code {}", exit_status);
  } else {
    pdp_error("Gdb unknown termination state");
  }
  // TODO: Do this gracefully?
  PDP_UNREACHABLE("Gdb child process exited");
}

};  // namespace pdp

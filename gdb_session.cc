#include "gdb_session.h"

#include "core/check.h"
#include "core/likely.h"
#include "core/log.h"
#include "parser/parser.h"

#include <fcntl.h>
#include <poll.h>
#include <unistd.h>

namespace pdp {

// TODO testing
void DebugFormatExpr(ExprBase *expr, StringBuilder<> &builder) {
  if (expr->kind == ExprBase::kString) {
    StringSlice s((char *)expr + sizeof(ExprString), expr->size);
    builder.Append(s);
  } else if (expr->kind == ExprBase::kList) {
    if (expr->size == 0) {
      builder.Append("[]");
    } else {
      builder.Append('[');
      ExprBase **elements = reinterpret_cast<ExprBase **>((char *)expr + sizeof(ExprList));
      DebugFormatExpr(elements[0], builder);
      for (size_t i = 1; i < expr->size; ++i) {
        builder.Append(", ");
        DebugFormatExpr(elements[i], builder);
      }
      builder.Append(']');
    }
  } else if (expr->kind == ExprBase::kTuple) {
    if (expr->size == 0) {
      builder.Append("{}");
    } else {
      ExprTuple *tuple = static_cast<ExprTuple *>(expr);
      ExprTuple::Result *results = tuple->results;
      builder.Appendf("{{}=", results[0].key);
      DebugFormatExpr(results[0].value, builder);
      for (size_t i = 1; i < expr->size; ++i) {
        builder.Appendf(", {}=", results[i].key);
        DebugFormatExpr(results[i].value, builder);
      }
      builder.Append('}');
    }
  } else {
    pdp_assert(false);
  }
}

GdbSession::GdbSession(Callback async_callback, Callback stream_callback)
    : in{0, 0}, out{0, 0}, err{0, 0}, token_counter(1), disconnected(false) {}

GdbSession::~GdbSession() {
  if (in[1] > 0) {
    close(in[1]);
  }
  if (out[0] > 0) {
    close(out[0]);
  }
  if (err[1] > 0) {
    close(err[1]);
  }
}

void GdbSession::Start() {
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

  int flags = fcntl(out[0], F_GETFL, 0);
  int ret = fcntl(out[0], F_SETFL, flags | O_NONBLOCK);
  CheckAndTerminate(ret, "fcntl");
}

void GdbSession::SendCommand(const StringSlice &command, Callback cb) {
  StringBuilder builder;
  builder.Append(token_counter);
  builder.Append(command);
  builder.Append('\n');

  // TODO pushback nullptr in callback and don't use vector pls
  callbacks.push_back(std::move(cb));
  ++token_counter;

  ssize_t total_written = 0;
  ssize_t remaining = builder.Size();
  while (remaining > 0) {
    ssize_t ret = write(in[1], builder.Data() + total_written, remaining);
    if (ret < 0) {
      Check(ret, "write");
      std::terminate();
    }
    total_written += ret;
    remaining -= ret;
  }
}

bool GdbSession::Poll(std::chrono::milliseconds ms) {
  struct pollfd poll_args;
  poll_args.fd = out[0];
  poll_args.events = POLLIN;

  int ret = poll(&poll_args, 1, ms.count());
  if (ret <= 0) {
    Check(ret, "poll");
    return false;
  }

  if (poll_args.revents & (POLLHUP | POLLERR)) {
    pdp_error("GDB session disconnected!");
    disconnected = true;
    return false;
  }

  // TODO returning is yikes right now
  if (poll_args.revents & POLLIN) {
    Process();
    return true;
  } else {
    return false;
  }
}

inline char IsStreamChar(char c) { return c == '~' || c == '@' || c == '&'; }

// TODO
// inline char IsAsyncChar(char c) { return

inline StringSlice ProcessStreamInPlace(char *begin, char *end) {
#if 0
  pdp_assert(end - begin > 0);
  pdp_assert(IsStreamChar(*begin));

  if (PDP_UNLIKELY(end - begin < 2 || begin[1] != '"')) {
    pdp_error("Unexpected start of stream message");
    return StringSlice(begin, end);
  }

  begin += 2;

  // Optimization pass, skip to first copy required.
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
      switch (read_head[1]) {
        case 'n':
          *write_head = '\n';
          break;
        case 't':
          *write_head = ' ';
          break;
        case '\\':
          *write_head = '\\';
          break;
        case 'r':
          break;
        case '"':
        default:
          *write_head = read_head[1];
      }
      write_head += (read_head[1] != 'r');
      read_head += 2;
    }
  }
  return StringSlice(begin, write_head + 1);
#endif
  // TODO
  return StringSlice(nullptr, nullptr);
}

void GdbSession::Process() {
  size_t n = buffer.ReadFull(out[0]);
  if (n <= 0) {
    pdp_warning("Could not read data after polling!");
    return;
  }

  auto s = buffer.ConsumeLine();
  while (!s.Empty()) {
    if (s[0] == '=' || s[0] == '*') {
      StringSlice ddz(s.Find(',') + 1, s.End() - 1);
      pdp_info("Parsing: {}", ddz);
      FirstPass first_pass(ddz);
      bool result = first_pass.Parse();
      if (result) {
        SecondPass final_pass(ddz, first_pass);
        ExprBase *expr = final_pass.Parse();
        StringBuilder debug_builder;
        DebugFormatExpr(expr, debug_builder);
        pdp_info("RES: {}", debug_builder.GetSlice());
      } else {
        pdp_error("Parse failed!");
      }
      // pdp_info("Parse result: {}", result);
      // pdp_info("Other: {}", s);
    } else {
      pdp_info("Got: {}", s);
    }
    s = buffer.ConsumeLine();
  }
}

};  // namespace pdp

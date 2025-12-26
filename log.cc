#include "log.h"
#include "check.h"

#include <cstdio>
#include <cstring>

/// @brief Holds the active log level for console messages
/// @note Each process will get a different copy of this variable.
static std::atomic_int console_level = static_cast<int>(pdp::Level::kInfo);

static const char *LogLevelToString(pdp::Level lvl) {
  switch (lvl) {
    case pdp::Level::kTrace:
      return "\e[37mtrace\e[0m";
    case pdp::Level::kInfo:
      return "\e[32minfo\e[0m";
    case pdp::Level::kWarn:
      return "\e[33m\e[1mwarning\e[0m";
    case pdp::Level::kError:
      return "\e[31m\e[1merror\e[0m";
    case pdp::Level::kCrit:
      return "\e[1m\e[41mcritical\e[0m";
    default:
      pdp_silent_assert(false);
      return "???";
  }
}

static const char *GetBasename(const char *name) {
  const char *basename = strrchr(name, '/');
  return (basename != nullptr) ? basename + 1 : name;
}

namespace pdp {

void Log(const char *file, unsigned line, Level lvl, const StringSlice &msg) {
  const size_t max_msg_length = 65535;
  auto length = msg.Size() > max_msg_length ? max_msg_length : msg.Size();

  pdp_silent_assert(lvl == Level::kTrace || console_level.load() <= static_cast<int>(lvl));

  auto now = std::chrono::system_clock::now();
  time_t epoch = std::chrono::system_clock::to_time_t(now);
  auto fraction = now - std::chrono::time_point_cast<std::chrono::seconds>(now);
  auto milli = std::chrono::duration_cast<std::chrono::milliseconds>(fraction).count();

  struct tm tm;
  memset(&tm, 0, sizeof(tm));
  localtime_r(&epoch, &tm);

  // NOTE: It's important to be in one function call to avoid race conditions!
  printf("[%d-%02d-%02d %02d:%02d:%02d.%03ld] [%s] [%s:%d] %.*s\n", tm.tm_year + 1900,
         tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec, milli, LogLevelToString(lvl),
         GetBasename(file), line, static_cast<int>(length), msg.Data());
  fflush(stdout);
}

/// @brief Changes the log level process wide of console messages
void SetConsoleLogLevel(Level level) {
  // XXX: Trace level logging is controlled only via macros!
  pdp_silent_assert(level != Level::kTrace);
  console_level.store(static_cast<int>(level));
}

bool ShouldLogAt(Level level) {
  return console_level.load(std::memory_order_relaxed) <= static_cast<int>(level);
}

bool ShouldLogAtTime(Level level, std::atomic_int64_t *last_print,
                     std::chrono::milliseconds threshold) {
  if (!ShouldLogAt(level)) {
    return false;
  }

  auto then = last_print->load(std::memory_order_relaxed);
  auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
                 std::chrono::steady_clock::now().time_since_epoch())
                 .count();
  if (now - then < threshold.count()) {
    return false;
  }
  return last_print->compare_exchange_strong(then, now, std::memory_order_relaxed);
}

}  // namespace pdp

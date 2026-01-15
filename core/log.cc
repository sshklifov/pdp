#include "log.h"
#include "check.h"

#include <fcntl.h>
#include <unistd.h>
#include <cstring>
#include <ctime>

/// @brief Holds the active log level for console messages
/// @note Each process will get a different copy of this variable.
static std::atomic_int log_level = static_cast<int>(pdp::Level::kInfo);

/// @brief File descriptor used for log output.
/// @note This does not imply ownership of the file descriptor.
/// @note Each process has its own copy of this variable.
static std::atomic_int log_output_fd = -1;

namespace pdp {

static void TerminateHandler() {
  int fd = log_output_fd.exchange(-1);
  if (PDP_LIKELY(fd > 0)) {
    close(fd);
  }
}

void RedirectLogging(const char *filename) {
  int fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0644);
  if (PDP_UNLIKELY(fd < 0)) {
    PDP_UNREACHABLE("Failed to redirect logging!");
  }
  int old_fd = log_output_fd.exchange(fd);
  pdp_assert(old_fd < 0);

  std::set_terminate(TerminateHandler);
}

static bool ShouldLogAt(Level level) {
  return log_level.load(std::memory_order_relaxed) <= static_cast<int>(level);
}

/// @brief Returns an ANSI-colored string literal for the given log level.
static constexpr StringSlice LogLevelToString(Level level) {
  switch (level) {
    case Level::kTrace:
      return "\e[37mtrace\e[0m";
    case Level::kInfo:
      return "\e[32minfo\e[0m";
    case Level::kWarn:
      return "\e[33m\e[1mwarning\e[0m";
    case Level::kError:
      return "\e[31m\e[1merror\e[0m";
    case Level::kCrit:
      return "\e[1m\e[41mcritical\e[0m";
    default:
      pdp_assert(false);
      return "???";
  }
}

static constexpr size_t EstimateLogLevelSize() {
  constexpr Level levels[] = {Level::kTrace, Level::kInfo, Level::kWarn, Level::kError,
                              Level::kCrit};
  size_t max = 0;
  for (auto level : levels) {
    size_t curr = LogLevelToString(level).Size();
    if (curr > max) {
      max = curr;
    }
  }
  return max;
}

static void Pad2Unchecked(byte value, StringBuilder<OneShotAllocator> &out) {
  pdp_assert(value <= 99);
  out.AppendUnchecked(static_cast<char>('0' + value / 10));
  out.AppendUnchecked(static_cast<char>('0' + value % 10));
}

static void Pad3Unchecked(unsigned value, StringBuilder<OneShotAllocator> &out) {
  pdp_assert(value <= 999);
  auto dig3 = value / 100;
  out.AppendUnchecked(static_cast<char>('0' + dig3));
  Pad2Unchecked(value - dig3 * 100, out);
}

static void WriteLogHeader(const StringSlice &filename, unsigned line, Level level,
                           StringBuilder<OneShotAllocator> &out) {
  // Capture current wall-clock time with nanosecond precision.
  timespec ts;
  memset(&ts, 0, sizeof(timespec));
  clock_gettime(CLOCK_REALTIME, &ts);
  // Convert to millisecond precision.
  auto milli = ts.tv_nsec / 1'000'000;
  // Parse epoch time into day, month, year etc. fields.
  struct tm tm;
  memset(&tm, 0, sizeof(tm));
  localtime_r(&ts.tv_sec, &tm);

  // Write out timestamp

  out.AppendUnchecked('[');
  out.AppendUnchecked(tm.tm_year + 1900);
  out.AppendUnchecked('-');
  Pad2Unchecked(tm.tm_mon + 1, out);
  out.AppendUnchecked('-');
  Pad2Unchecked(tm.tm_mday, out);

  out.AppendUnchecked(' ');
  Pad2Unchecked(tm.tm_hour, out);
  out.AppendUnchecked(':');
  Pad2Unchecked(tm.tm_min, out);
  out.AppendUnchecked(':');
  Pad2Unchecked(tm.tm_sec, out);
  out.AppendUnchecked('.');
  Pad3Unchecked(milli, out);

  out.AppendUnchecked(']');
  out.AppendUnchecked(' ');

  // Add log severity

  out.AppendUnchecked('[');
  out.AppendUnchecked(LogLevelToString(level));
  out.AppendUnchecked(']');
  out.AppendUnchecked(' ');

  // Add location

  out.AppendUnchecked('[');
  out.AppendUnchecked(filename);
  out.AppendUnchecked(':');
  out.AppendUnchecked(line);
  out.AppendUnchecked(']');
  out.AppendUnchecked(' ');
}

void Log(const char *f, unsigned line, Level level, const StringSlice &fmt, PackedValue *args,
         uint64_t type_bits) {
  if (PDP_UNLIKELY(!ShouldLogAt(level))) {
    return;
  }
  StringSlice filename(f);
  StringBuilder<OneShotAllocator> builder;

  constexpr EstimateSize estimator;
  constexpr size_t est = estimator("[2026-01-02 11:42:27.380] [] [:] \n") + EstimateLogLevelSize();
  size_t capacity =
      est + estimator(line) + estimator(filename) + estimator(fmt) + RunEstimator(args, type_bits);
  builder.ReserveFor(capacity);

  WriteLogHeader(filename, line, level, builder);
  builder.AppendPackUnchecked(fmt, args, type_bits);
  builder.AppendUnchecked('\n');
  LogUnformatted(builder.GetSlice());
}

void LogUnformatted(const StringSlice &str) {
  const int fd = log_output_fd.load();

  // Safeguard against blasting the terminal with output.
  const ssize_t max_length = 65535;

  // Write buffer in a loop to handle partial writes from write(2).
  ssize_t num_written = 0;
  ssize_t remaining = str.Size() > max_length ? max_length : str.Size();
  do {
    ssize_t ret = write(fd, str.Data() + num_written, remaining);
    if (PDP_UNLIKELY(ret < 0)) {
      // Not much you can do except accepting the partial write and moving on.
      return;
    }
    num_written += ret;
    remaining -= ret;
  } while (PDP_UNLIKELY(remaining));
}

LogLevelRAII::LogLevelRAII(Level new_level) {
  pdp_assert(new_level != Level::kTrace);
  restored_level = log_level.exchange(static_cast<int>(new_level));
}

LogLevelRAII::~LogLevelRAII() { log_level.store(restored_level); }

}  // namespace pdp

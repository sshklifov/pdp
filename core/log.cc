#include "log.h"
#include "check.h"

#include <fcntl.h>
#include <sys/file.h>
#include <unistd.h>
#include <cstring>
#include <ctime>
#include <exception>

/// @brief Holds the active log level for console messages
/// @note Each process will get a different copy of this variable.
static std::atomic_int log_level = static_cast<int>(pdp::Level::kInfo);

/// @brief File descriptor used for log output.
/// @note This does not imply ownership of the file descriptor.
/// @note Each process has its own copy of this variable.
static std::atomic_int log_output_fd = -1;

// TODO remove atomics and accept asyncs.

namespace pdp {

static void TerminateHandler() {
  int fd = log_output_fd.exchange(-1);
  if (PDP_LIKELY(fd > 0)) {
    close(fd);
  }
}

bool WriteFully(int fd, const void *data, size_t bytes) {
  // Write buffer in a loop to handle partial writes from write(2).
  size_t num_written = 0;
  do {
    ssize_t ret = write(fd, static_cast<const byte *>(data) + num_written, bytes);
    if (PDP_UNLIKELY(ret < 0)) {
      // Not much you can do except accepting the partial write and moving on.
      return false;
    }
    num_written += BitCast<size_t>(ret);
    bytes -= BitCast<size_t>(ret);
  } while (PDP_UNLIKELY(bytes > 0));
  return true;
}

void RedirectLogging(const char *filename) {
  int fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0644);
  if (PDP_UNLIKELY(fd < 0)) {
    PDP_UNREACHABLE("Failed to redirect logging!");
  }
  RedirectLogging(fd);
}

void RedirectLogging(int fd) {
  if (flock(fd, LOCK_EX) < 0) {
    close(fd);
    PDP_UNREACHABLE("Failed to obtain an exclusive lock on log file!");
  }

  int old_fd = log_output_fd.exchange(fd);
  pdp_assert(old_fd < 0);
  std::set_terminate(TerminateHandler);
}

bool LockLogFile(int fd) { return flock(fd, LOCK_SH | LOCK_NB) == 0; }

static bool ShouldLogAt(Level level) {
  return log_level.load(std::memory_order_relaxed) <= static_cast<int>(level);
}

/// @brief Returns an ANSI-colored string literal for the given log level.
static constexpr StringSlice LogLevelToString(Level level) {
  switch (level) {
    case Level::kTrace:
      return "\e[36mtrace\e[0m";
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

static constexpr size_t EstimateHeaderSize() {
  constexpr size_t constexpr_estimate = ConstexprLength("[2026-01-02 11:42:27.380] [] [:] \n") +
                                        EstimateLogLevelSize() + EstimateSizeV<unsigned>;
  return constexpr_estimate;
}

template <typename Alloc>
void Pad2Unchecked(byte value, StringBuilder<Alloc> &out) {
  pdp_assert(value <= 99);
  out.AppendUnchecked(static_cast<char>('0' + value / 10));
  out.AppendUnchecked(static_cast<char>('0' + value % 10));
}

template <typename Alloc>
void Pad3Unchecked(unsigned value, StringBuilder<Alloc> &out) {
  pdp_assert(value <= 999);
  auto dig3 = value / 100;
  out.AppendUnchecked(static_cast<char>('0' + dig3));
  Pad2Unchecked(value - dig3 * 100, out);
}

template <typename Alloc>
void WriteLogHeader(const StringSlice &filename, unsigned line, Level level,
                    StringBuilder<Alloc> &out) {
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

  const size_t capacity =
      EstimateHeaderSize() + filename.Size() + fmt.Size() + RunEstimator(args, type_bits);
  builder.ReserveFor(capacity);

  WriteLogHeader(filename, line, level, builder);
  builder.AppendPackUnchecked(fmt, args, type_bits);
  builder.AppendUnchecked('\n');
  LogUnformatted(builder.ToSlice());
}

void LogUnreachable(const char *f, unsigned line, const StringSlice &fmt, PackedValue *args,
                    uint64_t type_bits) {
  StringSlice filename(f);
  StringBuilder<OneShotAllocator> builder;

  const size_t capacity =
      EstimateHeaderSize() + filename.Size() + fmt.Size() + RunEstimator(args, type_bits);
  builder.ReserveFor(capacity);

  WriteLogHeader(filename, line, Level::kCrit, builder);
  builder.AppendPackUnchecked(fmt, args, type_bits);
  builder.AppendUnchecked('\n');
  OnFatalError(f, line, builder.Data());
}

void LogMultiLine(const char *f, unsigned line, Level level, StringSlice msg) {
  if (PDP_UNLIKELY(!ShouldLogAt(level))) {
    return;
  }
  StringSlice filename(f);
  StringBuilder builder;
  builder.ReserveFor(EstimateHeaderSize() + filename.Size());

  WriteLogHeader(filename, line, level, builder);
  const size_t header_size = builder.Size();

  auto it = msg.MemChar('\n');
  while (PDP_LIKELY(it)) {
    it += 1;
    builder.Append(msg.GetLeft(it));
    LogUnformatted(builder.ToSlice());
    builder.Truncate(header_size);

    msg.DropLeft(it);
    it = msg.MemChar('\n');
  }
  if (PDP_UNLIKELY(!msg.Empty())) {
    builder.Append(msg.GetLeft(it));
    LogUnformatted(builder.ToSlice());
  }
}

void LogUnformatted(const StringSlice &str) {
  const int fd = log_output_fd.load();
  // Safeguard against blasting the terminal with output.
  const size_t max_length = 65535;
  WriteFully(fd, str.Data(), str.Size() < max_length ? str.Size() : max_length);
}

}  // namespace pdp

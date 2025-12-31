#include "log.h"
#include "check.h"

#include <unistd.h>
#include <cstring>

/// @brief Holds the active log level for console messages
/// @note Each process will get a different copy of this variable.
static std::atomic_int console_level = static_cast<int>(pdp::Level::kInfo);

namespace pdp {

static void Pad2Unchecked(unsigned char value, StringBuilder<OneShotAllocator> &out) {
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

/// @brief Changes the log level process wide of console messages
void SetConsoleLogLevel(Level level) {
  // XXX: Trace level logging is controlled only via macros!
  pdp_assert(level != Level::kTrace);
  console_level.store(static_cast<int>(level));
}

namespace impl {

void WriteLogHeader(const char *filename, unsigned line, Level level,
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

void FlushMessage(const StringBuilder<OneShotAllocator> &msg) {
  // Safeguard against blasting the terminal with output.
  const ssize_t max_length = 65535;

  // Write buffer in a loop to handle partial writes from write(2).
  ssize_t num_written = 0;
  ssize_t remaining = msg.Size() > max_length ? max_length : msg.Size();
  do {
    ssize_t ret = write(STDOUT_FILENO, msg.Data() + num_written, remaining);
    if (PDP_UNLIKELY(ret < 0)) {
      // Not much you can do except accepting the partial write and moving on.
      return;
    }
    num_written += ret;
    remaining -= ret;
  } while (PDP_UNLIKELY(remaining));
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

}  // namespace impl

}  // namespace pdp

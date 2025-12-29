#pragma once

#include "strings/string_builder.h"

#include <atomic>
#include <chrono>

namespace pdp {

/// @brief Returns a pointer to the basename of the path-like string.
constexpr const char *GetBasename(const StringSlice &name) {
  const char *basename = name.MemReverseChar('/');
  return (basename != nullptr) ? basename + 1 : name.Begin();
}

/// @brief Log message severity
enum class Level { kTrace, kInfo, kWarn, kError, kCrit, kOff };

/// @brief Changes the log level process wide of console messages
void SetConsoleLogLevel(Level level);

namespace impl {
/// @brief True if logs at specified level will take effect
///
/// This is mostly used internally by the macros for optimization. But it can be used to debug
/// problems with logs that are not showing.
bool ShouldLogAt(Level level);

/// @brief True if logs at specified time and level will take effect.
///
/// Updates the 'last_print' atomic. This is mostly used internally by the macros. Do not use
/// manually!
bool ShouldLogAtTime(Level level, std::atomic_int64_t *last_print,
                     std::chrono::milliseconds threshold);

/// @brief Formats message to stdout and to systemd
///
/// Does not throw. Use the macros instead.
/// @param loc Source location where message happened
/// @param lvl Message severity
/// @param msg The message
void Log(const char *filename, unsigned line, Level lvl, const StringSlice &msg);

// @brief Writes a log entry header (time, level, file, line) into the output buffer.
void WriteLogHeader(const char *file, unsigned line, Level level,
                    StringBuilder<OneShotAllocator> &out);

// @brief Emits the fully formatted log message.
void FlushMessage(const StringBuilder<OneShotAllocator> &msg);

/// @brief Returns an ANSI-colored string literal for the given log level.
constexpr StringSlice LogLevelToString(Level level) {
  switch (level) {
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
      pdp_assert(false);
      return "???";
  }
}

// @brief Returns the count of base-10 digits in n
///
/// 'constexpr' to allow compile-time precomputation of log header width.
constexpr size_t CountLineDigits(size_t n) {
  size_t digits = 1;
  while (n >= 10) {
    n /= 10;
    ++digits;
  }
  return digits;
}

// @brief Computes the exact byte length of a log header
///
// Assuming a fixed timestamp format and ANSI-colored level strings. 'constexpr' to enable
// compile-time buffer sizing.
constexpr size_t CountHeaderSize(const char *file, size_t line, Level level) {
  size_t length = StringSlice("[2025-12-29 15:57:04.865] [] [:] ").Size();
  length += LogLevelToString(level).Size();
  length += StringSlice(file).Size();
  length += CountLineDigits(line);
  return length;
}

/// @brief Formats message to stdout and to systemd
///
/// Does not throw. Use the macros instead.
/// @param header_size Precomputed exact size of header
/// @param loc Source location where message happened
/// @param lvl Message severity
/// @param fmt The message with special format rules
/// @param args Arguments to be placed in format string
template <typename... Args>
void Log(size_t header_size, const char *filename, unsigned line, Level level,
         const StringSlice &fmt, Args &&...args) {
  // Estimate total buffer size: header + newline + unformatted message body.
  size_t capacity = 1 + header_size + fmt.Size();
  // Estimate size for arguments as well.
  [[maybe_unused]]
  EstimateSize estimator;
  ((capacity += estimator(args)), ...);

  // Single-shot buffer sized to final capacity. Any reallocation is a bug.
  StringBuilder<OneShotAllocator> builder(capacity);

  // Emit log header; must produce exactly header_size bytes.
  WriteLogHeader(filename, line, level, builder);
  pdp_assert(header_size == builder.Length());

  // Append formatted message and flush to terminal.
  builder.AppendfUnchecked(fmt, std::forward<Args>(args)...);
  builder.AppendUnchecked('\n');
  FlushMessage(builder);
}

}  // namespace impl

}  // namespace pdp

/// The `ShouldLogAt` check is forced inside the macro in order to avoid evaluation of __VA_ARGS__,
/// when possible. The cost is 2 additional instructions for each log (negligible) and the overhead
/// of an additional dynamic function call (depending on LTO and final DSO).
#define PDP_LOG(level, ...)                                                    \
  do {                                                                         \
    if (__builtin_expect(pdp::impl::ShouldLogAt(level), true)) {               \
      constexpr const char *_fl = pdp::GetBasename(__FILE__);                  \
      constexpr size_t _sz = pdp::impl::CountHeaderSize(_fl, __LINE__, level); \
      pdp::impl::Log(_sz, _fl, __LINE__, level, __VA_ARGS__);                  \
    }                                                                          \
  } while (0)

/// @brief Disable atrace calls on macro level
///
/// 'atrace' can be used for debug prints, which will trigger a lot dynamic calls to 'log_at'
/// without logging anything (hopefully, as we don't want debug prints in production). A simple
/// optimization is to remove the calls on macro level. This reduces code size and avoids the
/// dynamic calls. As a downside, enabling 'atrace' is more complicated. See also 'SetConsoleLevel'.
#ifdef PDP_TRACE_MESSAGES
#define pdp_trace(...)                                                      \
  do {                                                                      \
    constexpr pdp::Level _lvl = pdp::Level::kTrace;                         \
    constexpr const char *_fl = pdp::GetBasename(__FILE__);                 \
    constexpr size_t _sz = pdp::impl::CountHeaderSize(_fl, __LINE__, _lvl); \
    pdp::impl::Log(_sz, _fl, __LINE__, _lvl, __VA_ARGS__);                  \
  } while (0)
#define pdp_trace_once(...)                                                   \
  do {                                                                        \
    static std::atomic_bool _once = false;                                    \
    if (_once.exchange(true) == false) {                                      \
      constexpr pdp::Level _lvl = pdp::Level::kTrace;                         \
      constexpr const char *_fl = pdp::GetBasename(__FILE__);                 \
      constexpr size_t _sz = pdp::impl::CountHeaderSize(_fl, __LINE__, _lvl); \
      pdp::impl::Log(_sz, _fl, __LINE__, _lvl, __VA_ARGS__);                  \
    }                                                                         \
  } while (0)
#else
#define pdp_trace(...) (void)0
#define pdp_trace_once(...) (void)0
#endif

#define pdp_info(fmt, ...) PDP_LOG(pdp::Level::kInfo, fmt, ##__VA_ARGS__)

#define pdp_warning(fmt, ...) PDP_LOG(pdp::Level::kWarn, fmt, ##__VA_ARGS__)

#define pdp_error(fmt, ...) PDP_LOG(pdp::Level::kError, fmt, ##__VA_ARGS__)

#define pdp_critical(fmt, ...) PDP_LOG(pdp::Level::kCrit, fmt, ##__VA_ARGS__)

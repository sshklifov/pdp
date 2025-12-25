#pragma once

#include "string_builder.h"

#include <atomic>
#include <chrono>

namespace pdp {

/// @brief Log message severity
enum class Level { kTrace, kDebug, kInfo, kWarn, kError, kCrit, kOff };

/// @brief Changes the log level process wide of console messages
void SetConsoleLogLevel(Level level);

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
void Log(const char *filename, unsigned line, Level lvl, const StringView &msg);

/// @brief Formats message to stdout and to systemd
///
/// Does not throw. Use the macros instead.
/// @param loc Source location where message happened
/// @param lvl Message severity
/// @param fmt The message with special format rules
/// @param args Arguments to be placed in format string
template <typename... Args>
void Log(const char *filename, unsigned line, Level lvl, const StringView &fmt, Args &&...args) {
  StringBuilder builder;
  builder.Appendf(fmt, std::forward<Args>(args)...);
  Log(filename, line, lvl, builder.ViewOnly());
}

}  // namespace pdp

/// The `log_at` check is forced inside the macro in order to avoid evaluation of __VA_ARGS__,
/// when possible. The cost is 2 additional instructions for each log (negligible) and the overhead
/// of an additional virtual function call.
#define PDP_LOG(level, ...)                             \
  do {                                                  \
    if (pdp::ShouldLogAt(level)) {                      \
      pdp::Log(__FILE__, __LINE__, level, __VA_ARGS__); \
    }                                                   \
  } while (0)

#define PDP_LOG_RATE_LIMITED(level, threshold, ...)            \
  do {                                                         \
    static std::atomic_int64_t last_print = 0;                 \
    if (pdp::ShouldLogAtTime(level, &last_print, threshold)) { \
      pdp::Log(__FILE__, __LINE__, level, __VA_ARGS__);        \
    }                                                          \
  } while (0)

/// @brief Disable atrace calls on macro level
///
/// 'atrace' can be used for debug prints, which will trigger a lot dynamic calls to 'log_at'
/// without logging anything (hopefully, as we don't want debug prints in production). A simple
/// optimization is to remove the calls on macro level. This reduces code size and avoids the
/// dynamic calls. As a downside, enabling 'atrace' is more complicated. See also 'SetConsoleLevel'.
#ifdef PDP_ENABLE_ATRACE
#define trace(...) PDP_LOG(pdp::Level::kTrace, __VA_ARGS__)
#define trace_every(...) PDP_LOG_RATE_LIMITED(pdp::Level::kTrace, __VA_ARGS__)
#else
#define trace(...) (void)0
#define trace_every(...) (void)0
#endif

#define debug(...) PDP_LOG(pdp::Level::kDebug, __VA_ARGS__)

#define info(...) PDP_LOG(pdp::Level::kInfo, __VA_ARGS__)

#define warning(...) PDP_LOG(pdp::Level::kWarn, __VA_ARGS__)

#define error(...) PDP_LOG(pdp::Level::kError, __VA_ARGS__)

#define critical(...) PDP_LOG(pdp::Level::kCrit, __VA_ARGS__)

#define debug_every(...) PDP_LOG_RATE_LIMITED(pdp::Level::kDebug, __VA_ARGS__)

#define info_every(...) PDP_LOG_RATE_LIMITED(pdp::Level::kInfo, __VA_ARGS__)

#define warning_every(...) PDP_LOG_RATE_LIMITED(pdp::Level::kWarn, __VA_ARGS__)

#define error_every(...) PDP_LOG_RATE_LIMITED(pdp::Level::kError, __VA_ARGS__)

#define critical_every(...) PDP_LOG_RATE_LIMITED(pdp::Level::kCrit, __VA_ARGS__)

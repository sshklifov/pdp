#pragma once

#include "strings/string_builder.h"

#include <atomic>

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

void Log(const char *filename, unsigned line, Level level, const StringSlice &fmt,
         PackedValue *args, uint64_t type_bits);

// TODO comments

/// @brief Formats message to stdout and to systemd
///
/// Does not throw. Use the macros instead.
/// @param header_size Precomputed exact size of header
/// @param loc Source location where message happened
/// @param level Message severity
/// @param fmt The message with special format rules
/// @param args Arguments to be placed in format string
template <typename... Args>
void Log(const char *filename, unsigned line, Level level, const StringSlice &fmt, Args &&...args) {
  auto packed_args = MakePackedArgs(std::forward<Args>(args)...);
  Log(filename, line, level, fmt, packed_args.slots, packed_args.type_bits);
}

#ifdef PDP_TRACE_MESSAGES
void LogUnformatted(const StringSlice &str);
#endif

}  // namespace pdp

// The constexpr variable is forced so GetBasename is computed at compile time and loaded directly
// to a register.
#define PDP_LOG(level, ...)                                 \
  do {                                                      \
    constexpr const char *_bn = pdp::GetBasename(__FILE__); \
    pdp::Log(_bn, __LINE__, level, __VA_ARGS__);            \
  } while (0)

/// @brief Disable atrace calls on macro level
///
/// 'atrace' can be used for debug prints, which will trigger a lot dynamic calls to 'log_at'
/// without logging anything (hopefully, as we don't want debug prints in production). A simple
/// optimization is to remove the calls on macro level. This reduces code size and avoids the
/// dynamic calls. As a downside, enabling 'atrace' is more complicated. See also 'SetConsoleLevel'.
#ifdef PDP_TRACE_MESSAGES
#define pdp_trace(...)                                        \
  do {                                                        \
    constexpr const char *_bn = pdp::GetBasename(__FILE__);   \
    pdp::Log(_bn, __LINE__, pdp::Level::kTrace, __VA_ARGS__); \
  } while (0)
#define pdp_trace_once(...)                                     \
  do {                                                          \
    static std::atomic_bool _once = false;                      \
    if (_once.exchange(true) == false) {                        \
      constexpr const char *_bn = pdp::GetBasename(__FILE__);   \
      pdp::Log(_bn, __LINE__, pdp::Level::kTrace, __VA_ARGS__); \
    }                                                           \
  } while (0)
#else
#define pdp_trace(...) (void)0
#define pdp_trace_once(...) (void)0
#endif

#define pdp_info(fmt, ...) PDP_LOG(pdp::Level::kInfo, fmt, ##__VA_ARGS__)

#define pdp_warning(fmt, ...) PDP_LOG(pdp::Level::kWarn, fmt, ##__VA_ARGS__)

#define pdp_error(fmt, ...) PDP_LOG(pdp::Level::kError, fmt, ##__VA_ARGS__)

#define pdp_critical(fmt, ...) PDP_LOG(pdp::Level::kCrit, fmt, ##__VA_ARGS__)

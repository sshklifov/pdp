#pragma once

#include <cstdlib>
#include <exception>

#ifdef PDP_ENABLE_ASSERT
#define pdp_assert(x)                     \
  do {                                    \
    bool value = (x);                     \
    if (!value) {                         \
      pdp_error("Assert failed: {}", #x); \
      abort();                            \
    }                                     \
  } while (0)
#define pdp_silent_assert(x)                             \
  do {                                                   \
    bool value = (x);                                    \
    if (!value) {                                        \
      pdp::OnSilentAssertFailed(__FILE__, __LINE__, #x); \
    }                                                    \
  } while (0)
#else
#define pdp_assert(x) (void)0
#define pdp_silent_assert(x) (void)0
#endif

namespace pdp {
/// @brief Handles assertion failures without using the logging system.
///
/// This function is intended for low-level assertion handling where invoking
/// the regular logger may recurse, allocate, or otherwise be unsafe (e.g. during
/// allocator failure, shutdown, or other fatal paths).
[[noreturn]]
void OnSilentAssertFailed(const char *file, unsigned line, const char *what);

/// @brief Unconditionally terminates execution with a custom assertion message.
///
/// This function is intended for fatal internal invariants that cannot be
/// expressed as a boolean condition in an assertion (e.g. unreachable code
/// paths or logically impossible states).
///
/// This is functionally equivalent to:
///   assert(false && message);
[[noreturn]]
void OnSilentAssertFailed(const char *what, const char *context, size_t context_size);

/// @brief Checks the outcome of a C-style function returning a negative status on failure.
/// If the result is negative reports an `errno` style error code.
/// @param result The returned status or resource of the function.
/// @param operation Name of the carried out operation for logging purposes.
/// @return True if the operation executed successfully and false otherwise.
bool Check(int result, const char *operation);

/// @brief Checks the outcome of a C-style function returning a pointer to resources.
/// If the pointer is invalid reports the error code set in `errno`.
/// @param result The returned resource of the function.
/// @param operation Name of the carried out operation for logging purposes.
/// @return True if the operation executed successfully and false otherwise.
bool Check(void *pointer, const char *operation);

/// @brief Checks the outcome of a C-style function and throws an exception if it's bad.
/// @tparam T Should match the first argument type of the above declared Check functions.
/// @param result The outcome of a C-style function.
/// @param operation Name of the carried out operation for logging purposes.
template <typename T>
void CheckAndTerminate(T result, const char *operation) {
  bool is_successful = Check(result, operation);
  if (!is_successful) {
    std::terminate();
  }
}

};  // namespace pdp

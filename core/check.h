#pragma once

#include "internals.h"

#include <cstddef>

#ifdef PDP_ENABLE_ASSERT
#define pdp_assert(x)                                                   \
  do {                                                                  \
    if (PDP_UNLIKELY(!((x)))) {                                         \
      pdp::OnFatalError(__FILE__, __LINE__, "Assertion " #x " failed"); \
    }                                                                   \
  } while (0)
#define pdp_assert_non_constexpr(x)   \
  do {                                \
    if (!PDP_CONSTEXPR_EVALUATED()) { \
      pdp_assert(x);                  \
    }                                 \
  } while (0)
#else
#define pdp_assert(x) (void)0
#define pdp_assert_non_constexpr(x) (void)0
#endif

#define PDP_UNREACHABLE(msg) ::pdp::OnFatalError(__FILE__, __LINE__, msg)

namespace pdp {
/// @brief Handles assertion failures without using the logging system.
///
/// This function is intended for low-level assertion handling where invoking
/// the regular logger may recurse, allocate, or otherwise be unsafe (e.g. during
/// allocator failure, shutdown, or other fatal paths).
[[noreturn]]
void OnFatalError(const char *file, unsigned line, const char *what);

// TODO comment
[[noreturn]]
void OnFatalError(const char *what, const char *value, size_t value_length);

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

};  // namespace pdp

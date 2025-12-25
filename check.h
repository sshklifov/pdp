#pragma once

#include <exception>

namespace pdp {

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

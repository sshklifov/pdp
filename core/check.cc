#include "check.h"
#include "log.h"

#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#include <cerrno>

namespace pdp {

void OnFatalError(const char *file, unsigned line, const char *what) {
  const char *head = "[*** PDP ERROR ***] Assertion '";
  write(STDERR_FILENO, head, strlen(head));
  write(STDERR_FILENO, what, strlen(what));

  const char *failed_in = "' failed in ";
  write(STDERR_FILENO, failed_in, strlen(failed_in));
  write(STDERR_FILENO, file, strlen(file));
  write(STDERR_FILENO, ":", 1);

  char dig[8];
  int i = 7;
  do {
    dig[i] = '0' + (line % 10);
    line /= 10;
    --i;
  } while (i >= 0 && line != 0);

  write(STDERR_FILENO, dig + i + 1, 7 - i);
  write(STDERR_FILENO, "\n", 1);

  std::terminate();
}

void OnFatalError(const char *what, const char *value, size_t value_length) {
  const char *pdp_error = "[*** PDP ERROR ***] ";
  write(STDERR_FILENO, pdp_error, strlen(pdp_error));
  write(STDERR_FILENO, what, strlen(what));

  const char *failed_with = " occured with: ";
  write(STDERR_FILENO, failed_with, strlen(failed_with));
  write(STDERR_FILENO, value, value_length);
  write(STDERR_FILENO, "\n", 1);

  std::terminate();
}

static StringSlice GetErrorDescription() { return StringSlice(strerrordesc_np(errno)); }

// Does nothing, but is useful for debugging.
void OnCheckFailed() {}

bool Check(int result, const char *operation) {
  bool is_successful = (0 <= result);
  if (PDP_UNLIKELY(!is_successful)) {
    pdp_error("'{}' returned '{}'. Error '{}': '{}'.", StringSlice(operation), result, errno,
              GetErrorDescription());
    OnCheckFailed();
  }
  return is_successful;
}

bool Check(void *pointer, const char *operation) {
  bool is_successful = (pointer != nullptr && pointer != MAP_FAILED);
  if (PDP_UNLIKELY(!is_successful)) {
    pdp_error("'{}' returned '{}'. Error '{}': '{}'.", StringSlice(operation), pointer, errno,
              GetErrorDescription());
    OnCheckFailed();
  }
  return is_successful;
}

};  // namespace pdp

#include "check.h"
#include "log.h"

#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#include <cerrno>

namespace pdp {

void OnSilentAssertFailed(const char *file, unsigned line, const char *what) {
  const char *pdp_error = "[*** PDP ERROR ***] Assertion '";
  write(STDERR_FILENO, pdp_error, strlen(pdp_error));
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

  abort();
}

void OnSilentAssertFailed(const char *what, const char *context, size_t n) {
  const char *pdp_error = "[*** PDP ERROR ***] ";
  write(STDERR_FILENO, pdp_error, strlen(pdp_error));
  write(STDERR_FILENO, what, strlen(what));

  const char *failed_with = " occured with: ";
  write(STDERR_FILENO, failed_with, strlen(failed_with));
  write(STDERR_FILENO, context, n);
  write(STDERR_FILENO, "\n", 1);

  abort();
}

bool Check(int result, const char *operation) {
  bool is_successful = (0 <= result);
  if (!is_successful) {
    pdp_error("'{}' returned '{}'. Error '{}': '{}'.", operation, result, errno,
              strerrordesc_np(errno));
  }
  return is_successful;
}

bool Check(void *pointer, const char *operation) {
  bool is_successful = (pointer != nullptr && pointer != MAP_FAILED);
  if (!is_successful) {
    pdp_error("'{}' returned '{}'. Error '{}': '{}'.", operation, pointer, errno,
              strerrordesc_np(errno));
  }
  return is_successful;
}

};  // namespace pdp

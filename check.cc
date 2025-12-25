#include "check.h"
#include "log.h"

#include <string.h>
#include <sys/mman.h>
#include <cerrno>

namespace pdp {

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

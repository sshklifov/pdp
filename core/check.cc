#include "check.h"
#include "backtrace.h"
#include "log.h"

#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#include <cerrno>
#include <exception>

namespace pdp {

static void PrintDecimal(unsigned value) {
  // Print a maximum of 8 characters (okay for line numbers).
  char dig[8];
  int i = 7;
  do {
    dig[i] = '0' + (value % 10);
    value /= 10;
    --i;
  } while (i >= 0 && value != 0);

  LogUnformatted(StringSlice(dig + i + 1, 7 - i));
}

void OnFatalError(const char *file, unsigned line, const char *what) {
#ifdef PDP_DEBUG_BUILD
  LogUnformatted("Backtrace:\n");
  const unsigned max_frames = 16;
  void *frames[max_frames];
  FramePointerBacktrace(frames, max_frames);
  PrintBacktrace(frames, max_frames);
#endif

  const char *head = "[*** PDP ERROR ***] ";
  LogUnformatted(head);
  LogUnformatted(what);

  const char *failed_in = " in ";
  LogUnformatted(failed_in);
  LogUnformatted(file);
  LogUnformatted(":");
  PrintDecimal(line);
  LogUnformatted("\n");

  std::terminate();
}

void OnFatalError(const char *what, const char *value, size_t value_length) {
  const char *pdp_error = "[*** PDP ERROR ***] ";
  LogUnformatted(pdp_error);
  LogUnformatted(what);

  const char *occured_with = " occured with: ";
  LogUnformatted(occured_with);
  LogUnformatted(StringSlice(value, value_length));
  LogUnformatted("\n");

  std::terminate();
}

static StringSlice GetErrorDescription() { return StringSlice(strerrordesc_np(errno)); }

// Does nothing, but is useful for debugging.
void OnCheckFailed() {
#ifdef PDP_DEBUG_BUILD
  LogUnformatted("Backtrace:\n");
  const unsigned max_frames = 16;
  void *frames[max_frames];
  FramePointerBacktrace(frames, max_frames);
  PrintBacktrace(frames, max_frames);
#endif
}

bool Check(int result, const char *operation) {
  bool is_successful = (0 <= result);
  if (PDP_UNLIKELY(!is_successful)) {
    OnCheckFailed();
    pdp_error("'{}' returned '{}'. Error '{}': '{}'.", StringSlice(operation), result, errno,
              GetErrorDescription());
  }
  return is_successful;
}

bool Check(const void *pointer, const char *operation) {
  bool is_successful = (pointer != nullptr && pointer != MAP_FAILED);
  if (PDP_UNLIKELY(!is_successful)) {
    OnCheckFailed();
    pdp_error("'{}' returned '{}'. Error '{}': '{}'.", StringSlice(operation), pointer, errno,
              GetErrorDescription());
  }
  return is_successful;
}

void CheckFatal(int result, const char *operation) {
  if (PDP_UNLIKELY(!Check(result, operation))) {
    std::terminate();
  }
}

void CheckFatal(const void *result, const char *operation) {
  if (PDP_UNLIKELY(!Check(result, operation))) {
    std::terminate();
  }
}

};  // namespace pdp

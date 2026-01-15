#pragma once

#include "debug_coordinator.h"

namespace pdp {

inline HandlerCoroutine HandleStream(DebugCoordinator *d, const StringSlice &msg) {
#ifdef PDP_TRACE_MESSAGES
  LogUnformatted(msg);
#endif

  bool exists = co_await d->BufExists(505);
  if (exists) {
    pdp_critical("EXISTS!");
  } else {
    pdp_critical("DOES NOT EXISTS!");
  }
  co_return;
}

}  // namespace pdp

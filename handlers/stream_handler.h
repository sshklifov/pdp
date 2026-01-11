#pragma once

#include "application/debug_coordinator.h"

namespace pdp {

struct HandleStream {
  void operator()(DebugCoordinator *_, const StringSlice &msg) {
#ifdef PDP_TRACE_MESSAGES
    LogUnformatted(msg);
#endif
  }
};

};  // namespace pdp

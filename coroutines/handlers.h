#pragma once

#include "debug_coordinator.h"

namespace pdp {

Coroutine ClearBreakpointSign(DebugCoordinator &d, FixedString in_id, bool should_delete);

Coroutine PlaceBreakpointSign(DebugCoordinator &d, FixedString id);

void FormatBreakpointMessage(DebugCoordinator &d, GdbExprView bkpt,
                             BreakpointTable::NoSuspendIterator it);

Coroutine HandleNewBreakpoint(DebugCoordinator &d, UniquePtr<ExprBase> expr);

void HandleThreadSelect(DebugCoordinator *d, UniquePtr<ExprBase> expr);

}  // namespace pdp

#pragma once

#include "debug_coordinator.h"

namespace pdp {

void ClearBreakpointSign(DebugCoordinator &d, const StringSlice &in_id, bool should_delete);

void FormatBreakpointMessage(DebugCoordinator &d, GdbExprView bkpt, const Breakpoint &br,
                             const StringSlice &id);

void HandleNewBreakpoint(DebugCoordinator &d, UniquePtr<ExprBase> expr);

void HandleBreakpointDelete(DebugCoordinator &d, UniquePtr<ExprBase> expr);

void HandleThreadSelect(DebugCoordinator *d, UniquePtr<ExprBase> expr);

}  // namespace pdp

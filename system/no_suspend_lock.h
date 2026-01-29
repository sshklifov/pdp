#pragma once

#include "core/check.h"
#include "data/non_copyable.h"

namespace pdp {

struct NoSuspendLock {
  static void Lock() { ++depth; }

  static void Unlock() {
    pdp_assert(depth > 0);
    --depth;
  }

  static void CheckLocked() { pdp_assert(depth > 0); }

  static void CheckUnlocked() { pdp_assert(depth == 0); }

 private:
  static int depth;
};

struct NoSuspendGuard : public NonCopyableNonMovable {
  NoSuspendGuard() { NoSuspendLock::Lock(); }

  ~NoSuspendGuard() { NoSuspendLock::Unlock(); }
};

}  // namespace pdp

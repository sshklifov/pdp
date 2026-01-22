#pragma once

#include "core/check.h"
namespace pdp {

#ifdef PDP_ENABLE_ASSERT
struct OnceGuard {
  OnceGuard() : value(false) {}

  void Set() {
    pdp_assert(!value);
    value = true;
  }

  void Reset() {
    pdp_assert(value);
    value = false;
  }

 private:
  bool value;
};
#else
struct OnceGuard {
  void Set() {}
  void Reset() {}
};
#endif

};  // namespace pdp

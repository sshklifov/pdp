#pragma once

// TODO use more once guard and monotonic checker

#include "core/check.h"

#include <cstdint>

namespace pdp {

#ifdef PDP_ENABLE_ASSERT
struct MonotonicCheck {
  MonotonicCheck(uint32_t v = 0) : value(v) {}

  void Set(uint32_t next_value) {
    pdp_assert(value < next_value);
    value = next_value;
  }

 private:
  uint32_t value;
};
#else
struct MonotonicCheck {
  MonotonicCheck(uint32_t = 0) {}
  void Set(uint32_t) {}
};
#endif

};  // namespace pdp

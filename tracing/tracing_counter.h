#pragma once

#include "core/log.h"

namespace pdp {

template <size_t N>
struct TracingCounter {
  TracingCounter(const char *const (&n)[N]) : names(n), counters{}, next_print(print_every) {}

  void Count(size_t i) {
    pdp_assert(i < N);
    counters[i] += 1;
    --next_print;

    if (next_print <= 0) {
      next_print = print_every;
      size_t total = counters[0];
      for (size_t i = 1; i < N; ++i) {
        total += counters[i];
      }
      for (size_t i = 0; i < N; ++i) {
        pdp_trace("Counter '{}': {}/{}", StringSlice(names[i]), counters[i], total);
      }
    }
  }

 private:
  static constexpr unsigned print_every = 100;

  const char *const (&names)[N];
  unsigned counters[N];
  unsigned next_print;
};

}  // namespace pdp

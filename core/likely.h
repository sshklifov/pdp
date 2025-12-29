#pragma once

#include "log.h"

#include <atomic>

#ifdef PDP_TRACE_BRANCH
#define PDP_LIKELY(x)                                                              \
  [](bool value, const char *cond) -> bool {                                       \
    static std::atomic_uint total(0);                                              \
    static std::atomic_uint taken(0);                                              \
    total.fetch_add(1, std::memory_order_relaxed);                                 \
    if (value) {                                                                   \
      taken.fetch_add(1, std::memory_order_relaxed);                               \
    } else {                                                                       \
      auto now_total = total.load(std::memory_order_relaxed);                      \
      auto now_mispred = now_total - taken.load(std::memory_order_relaxed);        \
      pdp_trace("LIKELY '{}' misprediction: {}/{}", cond, now_mispred, now_total); \
    }                                                                              \
    return value;                                                                  \
  }(x, #x)
#define PDP_UNLIKELY(x)                                                              \
  [](bool value, const char *cond) -> bool {                                         \
    static std::atomic_uint total(0);                                                \
    static std::atomic_uint not_taken(0);                                            \
    total.fetch_add(1, std::memory_order_relaxed);                                   \
    if (!value) {                                                                    \
      not_taken.fetch_add(1, std::memory_order_relaxed);                             \
    } else {                                                                         \
      auto now_total = total.load(std::memory_order_relaxed);                        \
      auto now_mispred = now_total - not_taken.load(std::memory_order_relaxed);      \
      pdp_trace("UNLIKELY '{}' misprediction: {}/{}", cond, now_mispred, now_total); \
    }                                                                                \
    return value;                                                                    \
  }(x, #x)
#else
#define PDP_LIKELY(x) __builtin_expect(static_cast<bool>(x), true)
#define PDP_UNLIKELY(x) __builtin_expect(static_cast<bool>(x), false)
#endif

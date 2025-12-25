#pragma once

#ifdef NDEBUG
#define PDP_LIKELY(x) __builtin_expect(static_cast<bool>(x), true)
#else
#define PDP_LIKELY(x)                                            \
  [](bool value) -> bool {                                       \
    static std::atomic_uint total(0);                            \
    static std::atomic_uint taken(0);                            \
    total.fetch_add(1, std::memory_order_relaxed);               \
    if (value) {                                                 \
      taken.fetch_add(1, std::memory_order_relaxed);             \
    } else {                                                     \
      auto now_total = total.load(std::memory_order_relaxed);    \
      auto now_taken = taken.load(std::memory_order_relaxed);    \
      auto now_mispred = now_total - now_taken;                  \
      pdp_debug("LIKELY failed: {}/{}", now_mispred, now_total); \
    }                                                            \
    return value;                                                \
  }()(x)
#endif

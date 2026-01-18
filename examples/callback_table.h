#pragma once

#include "allocator.h"
#include "core/check.h"
#include "core/log.h"
#include "tracing/tracing_counter.h"

// TODO: add an optimization where you cache the last bucket found
// because a lot of times, i will just queue 100 request and these can be sequential
// TODO: also idea: allow only queue like entering -> exitting. process things in order
// and use this for a more optimal container.
// TODO: also idea: i split the bind call into two calls: one which does the binding and the other
// which constructs the callback.

namespace pdp {

template <typename... FunArgs>
struct SmallCapture {
  static constexpr unsigned StorageSize = 24;
  using InvokeFun = void (*)(void *, FunArgs...);

  template <typename... A>
  void operator()(A &&...args) {
    pdp_assert(invoke);
    invoke((void *)storage, std::forward<A>(args)...);
#ifdef PDP_ENABLE_ASSERT
    invoke = nullptr;
#endif
  }

  template <typename Callable>
  static void InvokeImpl(void *obj, FunArgs... args) {
    (*static_cast<Callable *>(obj))(static_cast<FunArgs &&>(args)...);
    (*static_cast<Callable *>(obj)).~Callable();
  }

  alignas(std::max_align_t) byte storage[StorageSize];
  InvokeFun invoke;
};

template <typename... FunArgs>
struct CallbackTable {
  static constexpr const size_t max_elements = 16'384;
  static constexpr const size_t default_elements = 8;

  using Capture = SmallCapture<FunArgs...>;

#ifdef PDP_TRACE_CALLBACK_TABLE
  CallbackTable() : trace_search(names) {
#else
  CallbackTable() {
#endif
    capacity = default_elements;
    table = Allocate<Capture>(allocator, capacity);
    indices = Allocate<uint32_t>(allocator, capacity);
    memset(indices, -1, capacity * sizeof(uint32_t));
  }

  ~CallbackTable() {
#ifdef PDP_ENABLE_ASSERT
    // XXX: Might leak memory since we don't call the destructor. The callback table lifetime is
    // that of the program, so it's fine.
    for (size_t i = 0; i < capacity; ++i) {
      if (indices[i] != invalid_id) {
        pdp_warning("Callback {} never invoked!", indices[i]);
      }
    }
#endif
    Deallocate<Capture>(allocator, table);
    Deallocate<uint32_t>(allocator, indices);
  }

  template <typename C, typename... A>
  void Bind(uint32_t id, A &&...args) {
    static_assert(sizeof(C) <= Capture::StorageSize, "Callback object too large");
#ifdef PDP_ENABLE_ASSERT
    for (size_t i = 0; i < capacity; ++i) {
      pdp_assert(indices[i] != id);
    }
#endif
    size_t i = 0;
    while (i < capacity) {
      if (PDP_UNLIKELY(indices[i] == invalid_id)) {
        break;
      }
      ++i;
    }
    if (PDP_UNLIKELY(i == capacity)) {
      Grow();
    }
    auto *cb = table + i;
    new (cb->storage) C(std::forward<A>(args)...);
    indices[i] = id;
    cb->invoke = &Capture::template InvokeImpl<C>;
#ifdef PDP_TRACE_CALLBACK_TABLE
    AccumulateHitPosition(i);
#endif
  }

  template <typename... A>
  bool Invoke(uint32_t id, A &&...args) {
    for (size_t i = 0; i < capacity; ++i) {
      if (PDP_UNLIKELY(indices[i] == id)) {
#ifdef PDP_TRACE_CALLBACK_TABLE
        AccumulateHitPosition(i);
#endif
        table[i](std::forward<A>(args)...);
        indices[i] = invalid_id;
        return true;
      }
    }
    pdp_warning("Could not invoke with id={}, not found!", id);
    return false;
  }

 private:
#ifdef PDP_TRACE_CALLBACK_TABLE
  void AccumulateHitPosition(size_t hit_pos) {
    if (hit_pos <= 5) {
      trace_search.Count(kSearchFiveOrLess);
    } else if (hit_pos <= 20) {
      trace_search.Count(kSearchTwentyOrLess);
    } else {
      trace_search.Count(kSearchMore);
    }
  }
#endif

  void Grow() {
    size_t half_capacity = capacity / 2;

    pdp_assert(capacity + half_capacity <= max_elements);
    table = Reallocate<Capture>(allocator, table, capacity + half_capacity);
    indices = Reallocate<uint32_t>(allocator, indices, capacity + half_capacity);
    memset(indices + capacity, -1, sizeof(uint32_t) * half_capacity);
    capacity += half_capacity;
    pdp_assert(table);
    pdp_assert(indices);
  }

  Capture *table;
  uint32_t *indices;
  size_t capacity;
  DefaultAllocator allocator;

  static constexpr uint32_t invalid_id = -1;

#ifdef PDP_TRACE_CALLBACK_TABLE
  enum Counters { kSearchFiveOrLess, kSearchTwentyOrLess, kSearchMore, kTotal };
  static constexpr const char *names[kTotal] = {"Hit in <= 5 iterations", "Hit in <= 20 iterations",
                                                "Hit in more iterations"};
  TracingCounter<kTotal> trace_search;
#endif
};

}  // namespace pdp

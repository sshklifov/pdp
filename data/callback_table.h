#include <cstdint>

#include "allocator.h"
#include "core/check.h"
#include "core/log.h"
#include "tracing/tracing_counter.h"

// TODO: add an optimization where you cache the last bucket found
// because a lot of times, i will just queue 100 request and these can be sequential
// TODO: also idea: allow only queue like entering -> exitting. process things in order
// and use this for a more optimal container.

namespace pdp {

template <typename Context>
struct SmallCallback {
  static constexpr unsigned StorageSize = 24;
  using InvokeFun = void (*)(void *, Context);

  static_assert(std::is_integral_v<Context> || std::is_pointer_v<Context>);

  void operator()(Context ctx) {
    pdp_assert(invoke);
    invoke((void *)storage, ctx);
#ifdef PDP_ENABLE_ASSERT
    invoke = nullptr;
#endif
  }

  template <typename Callable>
  static void InvokeImpl(void *obj, Context ctx) {
    (*static_cast<Callable *>(obj))(ctx);
    (*static_cast<Callable *>(obj)).~Callable();
  }

  alignas(std::max_align_t) unsigned char storage[StorageSize];
  InvokeFun invoke;
};

template <typename Context>
struct CallbackTable {
  static constexpr const size_t max_elements = 16'384;
  static constexpr const size_t default_elements = 8;

  using InvokeFun = typename SmallCallback<Context>::InvokeFun;

#ifdef PDP_TRACE_CALLBACK_TABLE
  CallbackTable() : trace_search(names) {
#else
  CallbackTable() {
#endif
    capacity = default_elements;
    table = Allocate<SmallCallback<Context>>(allocator, capacity);
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
    Deallocate<SmallCallback<Context>>(allocator, table);
    Deallocate<uint32_t>(allocator, indices);
  }

  template <typename Callable, typename... Args>
  void Bind(uint32_t id, Args &&...capture_args) {
    static_assert(sizeof(Callable) <= SmallCallback<Context>::StorageSize,
                  "Callback object too large");
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
    new (cb->storage) Callable(std::forward<Args>(capture_args)...);
    indices[i] = id;
    cb->invoke = &SmallCallback<Context>::template InvokeImpl<Callable>;
#ifdef PDP_TRACE_CALLBACK_TABLE
    AccumulateHitPosition(i);
#endif
  }

  bool Invoke(uint32_t id, Context ctx) {
    for (size_t i = 0; i < capacity; ++i) {
      if (PDP_UNLIKELY(indices[i] == id)) {
#ifdef PDP_TRACE_CALLBACK_TABLE
        AccumulateHitPosition(i);
#endif
        table[i](ctx);
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
    table = Reallocate<SmallCallback<Context>>(allocator, table, capacity + half_capacity);
    indices = Reallocate<uint32_t>(allocator, indices, capacity + half_capacity);
    memset(indices + capacity, -1, sizeof(uint32_t) * half_capacity);
    capacity += half_capacity;
    pdp_assert(table);
    pdp_assert(indices);
  }

  SmallCallback<Context> *table;
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

#pragma once

#include "core/check.h"

#include <utility>

namespace pdp {

template <typename... FunArgs>
struct Callback {
  // TODO!
  // static_assert((std::is_reference_v<FunArgs> && ...));

  // TODO static assert no reference / double reference shenanigans
  static constexpr unsigned StorageSize = 24;
  using InvokeFun = void (*)(void *, FunArgs &&...);

  Callback() {
#ifdef PDP_ENABLE_ASSERT
    invoke = nullptr;
#endif
  }

  template <typename C>
  void Assign(C &&callable) {
    static_assert(std::is_trivially_destructible_v<C>);
    static_assert(sizeof(C) <= StorageSize);
    new (storage) C(std::forward<C>(callable));
    pdp_assert(!invoke);
    invoke = &InvokeImpl<C>;
  }

  void operator()(FunArgs... args) {
    pdp_assert(invoke);
    invoke((void *)storage, std::forward<FunArgs>(args)...);
  }

 private:
  template <typename Callable>
  static void InvokeImpl(void *obj, FunArgs &&...args) {
    (*static_cast<Callable *>(obj))(std::forward<FunArgs>(args)...);
  }

  alignas(std::max_align_t) byte storage[StorageSize];
  InvokeFun invoke;
};

}  // namespace pdp

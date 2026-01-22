#pragma once

#include "core/check.h"

#include <utility>

namespace pdp {

template <typename... FunArgs>
struct SmallCapture {
  static constexpr unsigned StorageSize = 24;
  using InvokeFun = void (*)(void *, FunArgs...);

  SmallCapture() {
#ifdef PDP_ENABLE_ASSERT
    invoke = nullptr;
#endif
  }

  template <typename C, typename... Ca>
  void Bind(Ca &&...args) {
    static_assert(std::is_trivially_destructible_v<C>);
    static_assert(sizeof(C) <= StorageSize);
    new (storage) C(std::forward<Ca>(args)...);
    pdp_assert(!invoke);
    invoke = &InvokeImpl<C>;
  }

  template <typename... Fa>
  void operator()(Fa &&...args) {
    pdp_assert(invoke);
    invoke((void *)storage, std::forward<Fa>(args)...);
#ifdef PDP_ENABLE_ASSERT
    invoke = nullptr;
#endif
  }

 private:
  template <typename Callable>
  static void InvokeImpl(void *obj, FunArgs... args) {
    (*static_cast<Callable *>(obj))(static_cast<FunArgs &&>(args)...);
    (*static_cast<Callable *>(obj)).~Callable();
  }

  alignas(std::max_align_t) byte storage[StorageSize];
  InvokeFun invoke;
};

}  // namespace pdp

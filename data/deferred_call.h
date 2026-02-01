#pragma once

#include "core/check.h"

#include <utility>

namespace pdp {

template <typename... FunArgs>
struct DeferredCall {
  static constexpr unsigned StorageSize = 24;
  using InvokeFun = void (*)(void *, FunArgs...);
  using Storage = std::aligned_storage_t<StorageSize, alignof(std::max_align_t)>;

  DeferredCall() {
#ifdef PDP_ENABLE_ASSERT
    invoke = nullptr;
#endif
  }

  template <typename C, typename... Ca>
  void Bind(Ca &&...args) {
    static_assert(std::is_trivially_destructible_v<C>);
    static_assert(sizeof(C) <= StorageSize);
    static_assert(alignof(C) <= alignof(Storage));
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
  }

  Storage storage[StorageSize];
  InvokeFun invoke;
};

}  // namespace pdp

#pragma once

#include "data/allocator.h"
#include "data/non_copyable.h"

#include <type_traits>

namespace pdp {

template <typename T, typename Alloc = DefaultAllocator>
struct ScopedPtr : public NonCopyable {
  ScopedPtr(T *ptr = nullptr) noexcept : ptr(ptr) {}

  ScopedPtr(ScopedPtr &&rhs) noexcept : ptr(rhs.ptr) { rhs.ptr = nullptr; }

  void operator=(ScopedPtr &&rhs) = delete;

  ~ScopedPtr() {
    static_assert(std::is_void_v<T> || std::is_trivially_destructible_v<T>);
    allocator.DeallocateRaw(ptr);
  }

  operator bool() { return ptr != nullptr; }

  T *operator->() { return ptr; }

  const T *operator->() const { return ptr; }

 private:
  T *ptr;
  Alloc allocator;
};

// TODO move as a constructor ><
// template <typename T, typename... Args>
// ScopedPtr<T> MakeScopedPtr(Args &&...args) {
//   static_assert(std::is_nothrow_constructible_v<T, Args...>);

//   T *ptr = static_cast<T *>(malloc(sizeof(T)));
//   pdp_assert(ptr);
//   new (ptr) T(std::forward<Args>(args)...);
//   return ScopedPtr<T>(ptr);
// }

}  // namespace pdp

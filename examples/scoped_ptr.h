#pragma once

#include "allocator.h"

#include <type_traits>

namespace pdp {

template <typename T, typename Alloc = DefaultAllocator>
struct ScopedPtr {
  ScopedPtr(T *ptr = nullptr) noexcept : ptr(ptr) {}

  ScopedPtr(const ScopedPtr &rhs) = delete;

  ScopedPtr(ScopedPtr &&rhs) noexcept : ptr(rhs.ptr) { rhs.ptr = nullptr; }

  void operator=(const ScopedPtr &rhs) = delete;

  ScopedPtr &operator=(ScopedPtr &&rhs) noexcept {
    static_assert(std::is_trivially_destructible_v<T>);
    pdp_assert(this != &rhs);
    if (this != &rhs) {
      allocator.Deallocate(ptr);
      ptr = rhs.ptr;
      rhs.ptr = nullptr;
    }
    return (*this);
  }

  ~ScopedPtr() {
    static_assert(std::is_trivially_destructible_v<T>);
    allocator.Deallocate(ptr);
  }

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

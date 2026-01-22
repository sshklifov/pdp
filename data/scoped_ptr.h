#pragma once

#include "data/allocator.h"
#include "data/non_copyable.h"

#include <type_traits>

namespace pdp {

template <typename T, typename Alloc = DefaultAllocator>
struct ScopedPtr : public NonCopyable {
  ScopedPtr(T *ptr = nullptr, Alloc a = Alloc()) noexcept : ptr(ptr), allocator(a) {}

  template <typename U = T, std::enable_if_t<std::is_default_constructible_v<U>, int> = 0>
  ScopedPtr(size_t elements, Alloc a = Alloc()) noexcept : allocator(a) {
    pdp_assert(elements > 0);
    ptr = Allocate<T>(allocator, elements);
  }

  ScopedPtr(ScopedPtr &&rhs) noexcept : ptr(rhs.ptr), allocator(rhs.allocator) {
    rhs.ptr = nullptr;
  }

  void operator=(ScopedPtr &&rhs) = delete;

  ~ScopedPtr() {
    static_assert(std::is_void_v<T> || std::is_trivially_destructible_v<T>);
    allocator.DeallocateRaw(ptr);
  }

  operator bool() const { return ptr != nullptr; }

  T *operator->() { return ptr; }

  const T *operator->() const { return ptr; }

  template <typename U = T>
  std::enable_if_t<!std::is_void_v<U>, U &> operator*() {
    return *ptr;
  }

  template <typename U = T>
  std::enable_if_t<!std::is_void_v<U>, const U &> &operator*() const {
    return *ptr;
  }

  T *Get() { return ptr; }
  const T *Get() const { return ptr; }

 private:
  T *ptr;
  Alloc allocator;
};

}  // namespace pdp

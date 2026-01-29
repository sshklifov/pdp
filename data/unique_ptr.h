#pragma once

#include "data/allocator.h"
#include "data/non_copyable.h"

#include <type_traits>

namespace pdp {

template <typename T, typename Alloc = DefaultAllocator>
struct UniquePtr : public NonCopyable {
  UniquePtr(T *ptr = nullptr, Alloc a = Alloc()) noexcept : ptr(ptr), allocator(a) {}

  UniquePtr(UniquePtr &&rhs) noexcept : ptr(rhs.ptr), allocator(rhs.allocator) {
    rhs.ptr = nullptr;
  }

  void operator=(UniquePtr &&rhs) = delete;

  ~UniquePtr() {
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

  [[nodiscard]] T *Release() {
    T *res = ptr;
    ptr = nullptr;
    return res;
  }

 private:
  T *ptr;
  Alloc allocator;
};

template <typename T, typename Alloc = DefaultAllocator>
struct UniqueArray : public NonCopyable {
  template <typename U = T, std::enable_if_t<std::is_default_constructible_v<U>, int> = 0>
  UniqueArray(size_t max_elements, Alloc a = Alloc()) noexcept : allocator(a) {
    pdp_assert(max_elements > 0);
    ptr = Allocate<T>(allocator, max_elements);
  }

  UniqueArray(UniqueArray &&rhs) noexcept : ptr(rhs.ptr), allocator(rhs.allocator) {
    rhs.ptr = nullptr;
  }

  void operator=(UniqueArray &&rhs) = delete;

  ~UniqueArray() {
    static_assert(std::is_trivially_destructible_v<T>);
    allocator.DeallocateRaw(ptr);
  }

  operator bool() const { return ptr != nullptr; }

  T &operator[](size_t idx) { return ptr[idx]; }

  const T &operator[](size_t idx) const { return ptr[idx]; }

  T *Get() { return ptr; }
  const T *Get() const { return ptr; }

  [[nodiscard]] T *Release() {
    T *res = ptr;
    ptr = nullptr;
    return res;
  }

  void ShrinkToFit(size_t exact_elements) {
    pdp_assert(exact_elements <= allocator.GetAllocationSize(ptr));
    ptr = Reallocate<char>(allocator, ptr, exact_elements);
  }

 private:
  T *ptr;
  Alloc allocator;
};

using StringBuffer = UniqueArray<char>;

}  // namespace pdp

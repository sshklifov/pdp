#pragma once

#include "allocator.h"
#include "core/check.h"
#include "non_copyable.h"

#include <cstring>
#include <utility>

namespace pdp {

namespace impl {

template <typename T, typename Alloc>
struct _VectorPrivAcess;

}  // namespace impl

template <typename T, typename Alloc = DefaultAllocator>
struct Vector : public NonCopyable {
  static_assert(pdp::CanReallocate<T>::value, "T must be movable with realloc");
  static_assert(std::is_nothrow_destructible_v<T>, "T must be noexcept destructible");

  friend struct impl::_VectorPrivAcess<T, Alloc>;

  Vector(Alloc alloc = Alloc()) noexcept : ptr(nullptr), size(0), capacity(0), allocator(alloc) {}

  Vector(size_t cap, Alloc alloc = Alloc()) noexcept : size(0), capacity(cap), allocator(alloc) {
    pdp_assert(cap > 0);
    ptr = Allocate<T>(allocator, cap);
    pdp_assert(ptr);
  }

  Vector(Vector &&other)
      : ptr(other.ptr), size(other.size), capacity(other.capacity), allocator(other.allocator) {
    other.ptr = nullptr;
  }

  Vector &operator=(Vector &&other) = delete;

  ~Vector() {
    // TODO check disassembly here
    Destroy();
    // static_assert(std::is_trivially_destructible_v<T>);
    // Deallocate<T>(allocator, ptr);
  }

  const T *Data() const { return ptr; }
  T *Data() { return ptr; }

  const T *Begin() const { return ptr; }
  const T *End() const { return ptr + size; }

  T *Begin() { return ptr; }
  T *End() { return ptr + size; }

  T &First() {
    pdp_assert(!Empty());
    return *ptr;
  }

  T &Last() {
    pdp_assert(!Empty());
    return ptr[size - 1];
  }

  const T &First() const {
    pdp_assert(!Empty());
    return *ptr;
  }

  const T &Last() const {
    pdp_assert(!Empty());
    return ptr[size - 1];
  }

  bool Empty() const { return size == 0; }
  size_t Size() const { return size; }
  size_t Capacity() const { return capacity; }

  void Clear() { size = 0; }

  void Destroy() {
    // TODO is this only because of the tests?
    if constexpr (!std::is_trivially_destructible_v<T>) {
      for (size_t i = 0; i < size; ++i) {
        ptr[i].~T();
      }
    }
    Deallocate<T>(allocator, ptr);

    ptr = nullptr;
    size = 0;
    capacity = 0;
  }

  void ReserveFor(size_t new_elems) {
    pdp_assert(max_elements - new_elems >= size);
    size_t new_capacity = size + new_elems;
    if (new_capacity > capacity) {
      GrowExtra(new_capacity - capacity);
    }
  }

  Vector &operator+=(T &&elem) {
    ReserveFor(1);
    new (ptr + size) T(std::move(elem));
    ++size;
    return (*this);
  }

  template <typename U = T, std::enable_if_t<std::is_nothrow_constructible_v<U>, int> = 0>
  Vector &operator+=(const T &elem) {
    ReserveFor(1);
    new (ptr + size) T(elem);
    ++size;
    return (*this);
  }

  template <typename... Args>
  void Emplace(Args &&...args) {
    ReserveFor(1);
    new (ptr + size) T(std::forward<Args>(args)...);
    ++size;
  }

  T *NewElement() {
    ReserveFor(1);

    T *elem = ptr + size;
    static_assert(std::is_trivially_constructible_v<T>);
    ++size;
    return elem;
  }

  T &operator[](size_t index) {
    pdp_assert(index < Size());
    return ptr[index];
  }

  const T &operator[](size_t index) const {
    pdp_assert(index < Size());
    return ptr[index];
  }

 protected:
  void GrowExtra(size_t extra_elements) {
    size_t half_capacity = capacity / 2;
    if (half_capacity > extra_elements) {
      extra_elements = half_capacity;
    }

    [[maybe_unused]]
    const bool within_limits = max_elements - extra_elements >= capacity;
    pdp_assert(within_limits);
    capacity += extra_elements;

    ptr = Reallocate<T>(allocator, ptr, capacity);
    pdp_assert(ptr);
  }

  static constexpr const size_t max_elements = 1_GB / sizeof(T);

  T *ptr;
  size_t size;
  size_t capacity;

  Alloc allocator;
};

namespace impl {

template <typename T, typename Alloc>
struct _VectorPrivAcess {
  _VectorPrivAcess(Vector<T, Alloc> &v) : v(v) {}

  Vector<T, Alloc> &Get() { return v; }

  size_t Free() const { return v.capacity - v.size; }

  void Commit(size_t n) {
    pdp_assert(v.Size() + n <= v.Capacity());
    v.size += n;
  }

  [[nodiscard]] T *ReleaseData() {
    T *res = v.ptr;
    v.ptr = nullptr;
    return res;
  }

  bool IsHoldingData() { return v.ptr != nullptr; }

  void Reset() {
    v.ptr = nullptr;
    v.size = 0;
    v.capacity = 0;
  }

 private:
  Vector<T, Alloc> &v;
};

}  // namespace impl

template <typename T>
struct CanReallocate<Vector<T, DefaultAllocator>> : std::bool_constant<CanReallocate<T>::value> {};

};  // namespace pdp

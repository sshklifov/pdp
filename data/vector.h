#pragma once

#include "allocator.h"
#include "core/check.h"

#include <cstring>
#include <utility>

namespace pdp {

template <typename T, typename Alloc = DefaultAllocator>
struct Vector {
  static_assert(std::is_nothrow_move_constructible_v<T>, "T must be noexcept move constructible");
  static_assert(std::is_nothrow_copy_constructible_v<T>, "T must be noexcept copy constructible");
  static_assert(std::is_nothrow_destructible_v<T>, "T must be noexcept destructible");

  Vector() noexcept : ptr(nullptr), size(0), capacity(0) {}

  Vector(size_t cap, Alloc alloc = Alloc()) noexcept : size(0), capacity(cap), allocator(alloc) {
    pdp_assert(cap > 0);
    ptr = Allocate<T>(allocator, cap);
    pdp_assert(ptr);
  }

  Vector(Vector &&other)
      : ptr(other.ptr), size(other.size), capacity(other.capacity), allocator(other.allocator) {
    other.ptr = nullptr;
  }

  Vector(const Vector &) = delete;

  Vector &operator=(Vector &&other) {
    pdp_assert(this != &other);
    if (this != &other) {
      Deallocate<T>(allocator, ptr);
      ptr = other.ptr;
      size = other.size;
      capacity = other.capacity;
      allocator = other.allocator;

      other.ptr = nullptr;
    }
    return (*this);
  }

  void operator=(const Vector &) = delete;

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

  void Destroy() {
    static_assert(std::is_trivially_destructible_v<T>);
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

  Vector &operator+=(const T &elem) {
    ReserveFor(1);
    new (ptr + size) T(elem);
    ++size;
    return (*this);
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

    static_assert(std::is_trivially_move_constructible_v<T>);
    ptr = Reallocate<T>(allocator, ptr, capacity);
    pdp_assert(ptr);
  }

  static constexpr const size_t max_elements = 1_GB / sizeof(T);

  T *ptr;
  size_t size;
  size_t capacity;

  Alloc allocator;
};

};  // namespace pdp

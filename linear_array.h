#pragma once

#include "allocator.h"
#include "check.h"

#include <cstring>
#include <utility>

namespace pdp {

template <typename T, typename Alloc = DefaultAllocator>
struct LinearArray {
  static_assert(std::is_nothrow_move_constructible_v<T>, "T must be noexcept move constructible");
  static_assert(std::is_nothrow_destructible_v<T>, "T must be noexcept destructible");

  LinearArray(LinearArray &&other)
      : ptr(other.ptr), size(other.size), capacity(other.capacity), allocator(other.allocator) {
    other.ptr = nullptr;
  }

  LinearArray(const LinearArray &) = delete;

  LinearArray &operator=(LinearArray &&other) {
    pdp_assert(this != &other);
    if (this != &other) {
      ptr = other.ptr;
      size = other.size;
      capacity = other.capacity;
      allocator = other.allocator;

      other.ptr = nullptr;
    }
    return (*this);
  }

  void operator=(const LinearArray &) = delete;

  ~LinearArray() {
    static_assert(std::is_trivially_destructible_v<T>);
    Deallocate<T>(allocator, ptr, size);
  }

  const T *Data() const { return ptr; }
  T *Data() { return ptr; }

  const T *Begin() const { return ptr; }
  const T *End() const { return ptr + size; }

  T *Begin() { return ptr; }
  T *End() { return ptr + size; }

  bool Empty() const { return size == 0; }
  size_t Size() const { return size; }
  size_t Capacity() const { return capacity; }

  void ReserveFor(size_t new_elems) {
    pdp_assert(max_capacity - new_elems >= size);
    size_t new_capacity = size + new_elems;
    if (new_capacity > capacity) {
      GrowExtra(new_capacity - capacity);
    }
  }

  LinearArray &operator+=(T &&elem) {
    ReserveFor(1);
    new (ptr + size) T(std::move(elem));
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
  LinearArray(size_t cap, Alloc alloc = DefaultAllocator()) noexcept
      : size(0), capacity(cap), allocator(alloc) {
    pdp_assert(cap > 0);
    ptr = Allocate<T>(allocator, cap);
    pdp_assert(ptr);
  }

  void GrowExtra(const size_t req_capacity) {
    size_t half_capacity = capacity / 2;
    size_t grow_capacity = half_capacity > req_capacity ? half_capacity : req_capacity;

    const bool within_limits = max_capacity - grow_capacity >= capacity;
    pdp_assert(within_limits);
    capacity += grow_capacity;

    static_assert(std::is_trivially_move_constructible_v<T>);
    ptr = Reallocate<T>(allocator, ptr, capacity);
    pdp_assert(ptr);
  }

  static constexpr const size_t max_capacity = 1 << 30;

  T *ptr;
  size_t size;
  size_t capacity;

  Alloc allocator;
};

};  // namespace pdp

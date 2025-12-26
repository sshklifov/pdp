#pragma once

#include "check.h"

#include <cstdlib>
#include <cstring>
#include <limits>
#include <utility>

namespace pdp {

template <typename T>
struct LinearArray {
  static_assert(std::is_nothrow_move_constructible_v<T>, "T must be noexcept move constructible");
  static_assert(std::is_nothrow_destructible_v<T>, "T must be noexcept destructible");

  // LinearArray() : ptr(nullptr), size(0), capacity(0) {}

  LinearArray(size_t cap) noexcept : ptr(static_cast<T *>(malloc(cap))), size(0), capacity(cap) {
    pdp_silent_assert(cap > 0);
  }

  LinearArray(LinearArray &&other) : ptr(other.ptr), size(other.size), capacity(other.capacity) {
    other.ptr = nullptr;
  }

  LinearArray(const LinearArray &) = delete;

  LinearArray &operator=(LinearArray &&other) {
    if (this != &other) {
      ptr = other.ptr;
      size = other.size;
      capacity = other.capacity;
      other.ptr = nullptr;
    }
    return (*this);
  }

  void operator=(const LinearArray &) = delete;

  ~LinearArray() {
    // if constexpr (!std::is_trivially_destructible_v<T>) {
    //   pdp_trace_once("Detected non trivially destructible type in {}", __PRETTY_FUNCTION__);
    //   for (size_t i = 0; i < size; ++i) {
    //     delete (ptr + i);
    //   }
    // }
    // TODO?
    static_assert(std::is_trivially_destructible_v<T>);
    free(ptr);
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

  // void Reserve(size_t new_capacity) {
  //   if (new_capacity > capacity) {
  //     GrowExtra(new_capacity - capacity);
  //   }
  // }

  void ReserveFor(size_t new_elems) {
    pdp_silent_assert(max_capacity - new_elems >= size);
    size_t new_capacity = size + new_elems;
    if (new_capacity > capacity) {
      GrowExtra(new_capacity - capacity);
    }
  }

  LinearArray &operator+=(T &&elem) {
    pdp_silent_assert(size < max_capacity);
    ReserveFor(1);
    new (ptr + size) T(std::move(elem));
    ++size;
    return (*this);
  }

  T &operator[](size_t index) {
    pdp_silent_assert(index < Size());
    return ptr[index];
  }

  const T &operator[](size_t index) const {
    pdp_silent_assert(index < Size());
    return ptr[index];
  }

 protected:
  void GrowExtra(const size_t req_capacity) {
    size_t half_capacity = capacity / 2;
    size_t grow_capacity = half_capacity > req_capacity ? half_capacity : req_capacity;
    const bool no_overflow = max_capacity - grow_capacity >= capacity;
    pdp_silent_assert(no_overflow);

    // TODO?
    static_assert(std::is_trivially_move_constructible_v<T>);

    capacity += grow_capacity;
    ptr = static_cast<char *>(realloc(ptr, capacity));
    pdp_silent_assert(ptr);
  }

  static constexpr const auto max_capacity = std::numeric_limits<size_t>::max();

  T *ptr;
  size_t size;
  size_t capacity;
};

};  // namespace pdp

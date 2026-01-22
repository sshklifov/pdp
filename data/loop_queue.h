#pragma once

#include "allocator.h"
#include "core/check.h"

#include <utility>

namespace pdp {

template <typename T, typename Alloc = DefaultAllocator>
struct LoopQueue {
  LoopQueue(size_t power_of_two, Alloc alloc = Alloc()) : allocator(alloc) {
    pdp_assert(power_of_two > 0);
    pdp_assert((power_of_two & (power_of_two - 1)) == 0);

    ptr = Allocate<T>(allocator, power_of_two);
    mask = power_of_two - 1;
    begin = 0;
    size = 0;
  }

  ~LoopQueue() {
    if (!std::is_trivially_destructible_v<T>) {
      for (size_t i = 0; i < size; ++i) {
        // TODO will the compiler figure out to do add + mask?
        auto masked_index = (i + begin) & mask;
        ptr[masked_index].~T();
      }
    }
    Deallocate<T>(allocator, ptr);
  }

  bool Empty() const { return size == 0; }

  size_t Size() const { return size; }

  T &At(size_t index) {
    pdp_assert(index < Size());
    size_t masked_index = (begin + index) & mask;
    return ptr[masked_index];
  }

  const T &At(size_t index) const {
    pdp_assert(index < Size());
    size_t masked_index = (begin + index) & mask;
    return ptr[masked_index];
  }

  template <typename... Args>
  void EmplaceFront(Args &&...args) {
    if (PDP_UNLIKELY(size > mask)) {
      Grow();
    }
    begin = (begin + mask) & mask;
    new (ptr + begin) T(std::forward<Args>(args)...);
    ++size;
  }

  template <typename... Args>
  void EmplaceBack(Args &&...args) {
    if (PDP_UNLIKELY(size > mask)) {
      Grow();
    }
    auto end = (begin + size) & mask;
    new (ptr + end) T(std::forward<Args>(args)...);
    ++size;
  }

  T &Front() {
    pdp_assert(!Empty());
    return ptr[begin];
  }

  T &Back() {
    pdp_assert(!Empty());
    size_t end = (begin + size - 1) & mask;
    return ptr[end];
  }

  void PopFront() {
    pdp_assert(!Empty());
    if (std::is_trivially_destructible_v<T>) {
      ptr[begin].~T();
    }
    begin = (begin + 1) & mask;
    size--;
  }

 private:
  void Grow() {
    T *new_ptr = Allocate<T>(allocator, size * 2);
    for (size_t i = 0; i <= mask; ++i) {
      size_t masked_index = (begin + i) & mask;
      new (new_ptr + i) T(std::move(ptr[masked_index]));
    }
    Deallocate<T>(allocator, ptr);
    ptr = new_ptr;
    mask = (mask << 1) | 1;
    begin = 0;
  }

  static constexpr const size_t max_elements = 16'384;

  T *ptr;
  size_t mask;
  size_t begin;
  size_t size;
  Alloc allocator;
};

};  // namespace pdp

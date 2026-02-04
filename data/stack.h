#pragma once

#include "vector.h"

namespace pdp {

template <typename T, typename Alloc = DefaultAllocator>
struct Stack : public Vector<T, Alloc> {
  using Vector<T, Alloc>::Vector;

  void Push(T &&value) { (*this) += std::move(value); }

  void Push(const T &value) { (*this) += value; }

  void Pop() {
    pdp_assert(!this->Empty());
    --this->size;
    if constexpr (!std::is_trivially_destructible_v<T>) {
      this->ptr[this->size]->~T();
    }
  }

  T &Top() { return this->Last(); }

  const T &Top() const { return this->Last(); }
};

}  // namespace pdp

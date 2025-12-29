#pragma once

#include "linear_array.h"

namespace pdp {

template <typename T, typename Alloc = DefaultAllocator>
struct Stack : public LinearArray<T, Alloc> {
  using LinearArray<T, Alloc>::LinearArray;

  void Push(T &&value) { (*this) += std::move(value); }

  void Push(const T &value) { (*this) += value; }

  void Pop() {
    static_assert(std::is_trivially_destructible_v<T>);

    pdp_assert(!this->Empty());
    --this->size;
  }
};

}  // namespace pdp

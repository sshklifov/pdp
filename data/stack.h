#pragma once

#include "vector.h"

namespace pdp {

namespace impl {

template <typename Alloc>
struct _StackPrivAccess;

}  // namespace impl

template <typename T, typename Alloc = DefaultAllocator>
struct Stack : public Vector<T, Alloc> {
  // Required for chunk_array.cc only. So it is declared there.
  friend struct impl::_StackPrivAccess<Alloc>;

  using Vector<T, Alloc>::Vector;

  void Push(T &&value) { (*this) += std::move(value); }

  void Push(const T &value) { (*this) += value; }

  void Pop() {
    static_assert(std::is_trivially_destructible_v<T>);

    pdp_assert(!this->Empty());
    --this->size;
  }

  T &Top() { return this->Last(); }

  const T &Top() const { return this->Last(); }
};

}  // namespace pdp

#pragma once

#include "core/check.h"
#include "core/internals.h"
#include "data/allocator.h"
#include "data/non_copyable.h"

#include <coroutine>
#include <cstdint>

namespace pdp {

struct HandlerCoroutine : public NonCopyableNonMovable {
  struct promise_type {
    HandlerCoroutine get_return_object() noexcept {
      return HandlerCoroutine(std::coroutine_handle<promise_type>::from_promise(*this));
    }

    std::suspend_never initial_suspend() noexcept { return {}; }
    std::suspend_never final_suspend() noexcept { return {}; }

    void return_void() noexcept {}

    void unhandled_exception() noexcept { PDP_UNREACHABLE("Exception in coroutine"); }
  };

  explicit HandlerCoroutine(std::coroutine_handle<promise_type> c) : coro(c) {}

  ~HandlerCoroutine() = default;

 private:
  std::coroutine_handle<promise_type> coro;
};

struct CoroutineTable {
  static constexpr const size_t max_elements = 16'384;

  using Coroutine = std::coroutine_handle<HandlerCoroutine::promise_type>;

  CoroutineTable() {
    const size_t initial_capacity = 8;
    mask = initial_capacity - 1;
    table = Allocate<Coroutine>(allocator, initial_capacity);
    tokens = Allocate<uint32_t>(allocator, initial_capacity);
    begin = 0;
    size = 0;
    // memset(indices, -1, capacity * sizeof(uint32_t));
  }

  ~CoroutineTable() {
    // TODO
#if 0
#ifdef PDP_ENABLE_ASSERT
    // XXX: Might leak memory since we don't call the destructor. The callback table lifetime is
    // that of the program, so it's fine.
    for (size_t i = 0; i < capacity; ++i) {
      if (indices[i] != invalid_id) {
        pdp_warning("Callback {} never invoked!", indices[i]);
      }
    }
#endif
    Deallocate<Capture>(allocator, table);
    Deallocate<uint32_t>(allocator, indices);
#endif
  }

  // bool IsFull() const {
  //   if (PDP_LIKELY(begin == end)) {
  //     return false;
  //   }
  //   auto adv = (end + 1) & mask;
  //   return tokens[adv] > 0;
  // }

  void Suspend(uint32_t token, Coroutine coro) {
    if (PDP_UNLIKELY(size > mask)) {
      Grow();
    }

    auto pos = (begin + size) & mask;
    new (table + pos) Coroutine(coro);
    tokens[pos] = token;

#if PDP_ENABLE_ASSERT
    pdp_assert(token > 0);
    if (size) {
      auto prev = (pos + mask) & mask;
      pdp_assert(tokens[prev] < token);
    }
#endif
    ++size;
  }

  void Resume(uint32_t token) {
    pdp_assert(size > 0);
    if (PDP_UNLIKELY(token != tokens[begin])) {
      // TODO I WILL NOT PUSH ALL TOKENS! size == 0 checks everywhere
      // if (PDP_LIKELY(
      auto lo = begin;
      auto hi = begin + size;
      while (hi - lo > 1) {
        auto mid = ((lo + hi) << 1) & mask;
        auto token = tokens[mid] & UINT32_C(0x7FFFFFFF);
        if (token < tokens[mid]) {
          hi = mid;
        } else {
          lo = mid;
        }
      }
      pdp_assert(tokens[lo] == token);
      tokens[lo] |= UINT32_C(0xa0000000);
      return;
    }

    uint32_t pos = begin;
    begin = (begin + 1) & mask;
    size--;

    table[pos].resume();

    while (PDP_UNLIKELY(size > 0 && (tokens[begin] & UINT32_C(0xa0000000)))) {
      uint32_t pos = begin;
      begin = (begin + 1) & mask;
      size--;
      table[pos].resume();
    }
  }

 private:
  void Grow() {
    PDP_UNREACHABLE("Whoa whoa chill");
#if 0
    size_t half_capacity = capacity / 2;

    pdp_assert(capacity + half_capacity <= max_elements);
    table = Reallocate<Capture>(allocator, table, capacity + half_capacity);
    indices = Reallocate<uint32_t>(allocator, indices, capacity + half_capacity);
    memset(indices + capacity, -1, sizeof(uint32_t) * half_capacity);
    capacity += half_capacity;
    pdp_assert(table);
    pdp_assert(indices);
#endif
  }

  Coroutine *table;
  uint32_t *tokens;
  uint32_t mask;
  uint32_t begin;
  uint32_t size;
  DefaultAllocator allocator;
};

}  // namespace pdp

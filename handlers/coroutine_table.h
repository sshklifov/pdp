#pragma once

#include "core/check.h"
#include "core/internals.h"
#include "core/log.h"
#include "data/allocator.h"
#include "data/non_copyable.h"
#include "strings/string_builder.h"

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
  }

  ~CoroutineTable() {
    if (size > 0) {
      pdp_error("Suspended coroutines are going to be force destroyed!");
    }
    Deallocate<CoroutineTable>(allocator, table);
    Deallocate<uint32_t>(allocator, tokens);
  }

  void Suspend(uint32_t token, Coroutine coro) {
    if (PDP_UNLIKELY(size > mask)) {
      Grow();
    }

    auto pos = (begin + size) & mask;
    if (PDP_UNLIKELY(!Empty() && token < tokens[begin])) {
      begin = (begin + mask) & mask;
      pos = begin;
    }
    new (table + pos) Coroutine(coro);
    tokens[pos] = token;
    ++size;

    pdp_assert(pos == begin || tokens[pos - 1] < tokens[pos]);
    pdp_assert(pos == (begin + size - 1) % mask || tokens[pos] < tokens[pos + 1]);
  }

  bool Empty() const { return size == 0; }

  void Resume(uint32_t token) {
    if (PDP_LIKELY(!Empty())) {
      if (PDP_LIKELY(token == tokens[begin])) {
        auto resumed = table[begin];
        begin = (begin + 1) & mask;
        size--;
        resumed.resume();
        return;
      }
      pdp_assert(Empty() || token < tokens[begin]);
    }
  }

  void PrintSuspendedTokens() {
    pdp::StringBuilder builder;
    builder.Append("Suspended tokens: ");
    for (uint32_t i = 0; i < size; ++i) {
      uint32_t pos = (begin + i) & mask;
      builder.AppendFormat("{} ", tokens[pos]);
    }
    pdp_critical(builder.GetSlice());
  }

 private:
  void Grow() {
    auto new_table = Allocate<Coroutine>(allocator, size * 2);
    auto new_tokens = Allocate<uint32_t>(allocator, size * 2);
    for (size_t i = begin; i <= mask; ++i) {
      new (new_table + i) Coroutine(std::move(table[i]));
      new_tokens[i] = tokens[i];
    }
    auto offset = mask + 1 - begin;
    for (size_t i = 0; i < begin; ++i) {
      new (new_table + offset + i) Coroutine(std::move(table[i]));
      new_tokens[offset + i] = tokens[i];
    }
    Deallocate<Coroutine>(allocator, table);
    Deallocate<uint32_t>(allocator, tokens);
    table = new_table;
    tokens = new_tokens;
    mask = (mask << 1) | 1;
  }

  Coroutine *table;
  uint32_t *tokens;
  uint32_t mask;
  uint32_t begin;
  uint32_t size;
  DefaultAllocator allocator;
};

}  // namespace pdp

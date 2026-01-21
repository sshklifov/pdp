#pragma once

#include "core/check.h"
#include "core/log.h"
#include "data/allocator.h"
#include "data/non_copyable.h"

#include <coroutine>

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

struct SshTable {
  static constexpr const int max_elements = 4;

  using Coroutine = std::coroutine_handle<HandlerCoroutine::promise_type>;

  SshTable() {
    table = Allocate<Coroutine>(allocator, max_elements);
    descriptors = Allocate<int>(allocator, max_elements);
    memset(descriptors, -1, sizeof(int) * max_elements);
    size = 0;
  }

  ~SshTable() {
    if (size > 0) {
      pdp_error("Suspended SSH coroutines are going to be force destroyed!");
    }
    Deallocate<SshTable>(allocator, table);
    Deallocate<int>(allocator, descriptors);
  }

  // bool Empty() const { return size == 0; }

  // bool Full() const { return size == max_elements; }

  int FindCoroutineSlot() {
    // pdp_assert(fd >= 0);
    // pdp_assert(!Full());

    for (int i = 0; i < max_elements; ++i) {
      if (descriptors[i] < 0) {
        return i;
      }
    }
    return -1;
  }

  // void SuspendAt(int pos, int fd, Coroutine coro) {
  //   pdp_assert(pos >= 0 && descriptors[pos] < 0);
  //   new (table + pos) Coroutine(coro);
  //   descriptors[pos] = fd;
  // }

  bool Resume(int fd) {
    if (PDP_LIKELY(!Empty())) {
      for (size_t i = 0; i < size; ++i) {
        if (descriptors[i] == fd) {
          auto resumed = table[i];
          // TODO
        }
      }
      // if (PDP_LIKELY(token == tokens[begin])) {
      //   auto resumed = table[begin];
      //   begin = (begin + 1) & mask;
      //   size--;
      //   resumed.resume();
      //   return true;
      // }
    }
    return false;
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
  Coroutine *table;
  int *descriptors;
  size_t size;
  DefaultAllocator allocator;
};

}  // namespace pdp

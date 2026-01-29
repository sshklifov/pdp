#pragma once

#include "core/check.h"
#include "core/log.h"
#include "data/loop_queue.h"
#include "data/non_copyable.h"
#include "strings/string_builder.h"
#include "system/no_suspend_lock.h"

#include <coroutine>
#include <cstdint>

namespace pdp {

struct Coroutine : public NonCopyableNonMovable {
  struct promise_type {
    Coroutine get_return_object() noexcept {
      return Coroutine(std::coroutine_handle<promise_type>::from_promise(*this));
    }

    std::suspend_never initial_suspend() noexcept { return {}; }
    std::suspend_never final_suspend() noexcept { return {}; }

    void return_void() noexcept {}

    void unhandled_exception() noexcept { PDP_UNREACHABLE("Exception in coroutine"); }

    template <typename Awaitable>
    decltype(auto) await_transform(Awaitable &&a) {
      NoSuspendLock::CheckUnlocked();
      return std::forward<Awaitable>(a);
    }
  };

  explicit Coroutine(std::coroutine_handle<promise_type> c) : coro(c) {}

  ~Coroutine() = default;

 private:
  std::coroutine_handle<promise_type> coro;
};

struct CoroutineTokenTable {
  static constexpr const size_t max_elements = 16'384;

  using Handle = std::coroutine_handle<Coroutine::promise_type>;

  CoroutineTokenTable() : table(8) {}

  ~CoroutineTokenTable() {
    if (!table.Empty()) {
      pdp_error("Suspended handler coroutines are going to be force destroyed!");
    }
  }

  bool Empty() const { return table.Empty(); }

  void Suspend(uint32_t token, Handle coro) {
    if (PDP_UNLIKELY(!table.Empty() && token < table.Front().token)) {
      table.EmplaceFront(coro, token);
    } else {
      pdp_assert(table.Empty() || table.Back().token < token);
      table.EmplaceBack(coro, token);
    }
  }

  bool Resume(uint32_t token) {
    if (PDP_LIKELY(!table.Empty())) {
      if (PDP_LIKELY(token == table.Front().token)) {
        auto resumed = table.Front().coro;
        table.PopFront();
        resumed.resume();
        return true;
      }
    }
    pdp_assert(table.Empty() || token < table.Front().token);
    return false;
  }

  void PrintSuspendedTokens() const {
    pdp::StringBuilder builder;
    builder.Append("Suspended tokens: ");
    for (uint32_t i = 0; i < table.Size(); ++i) {
      builder.AppendFormat("{} ", table.At(i).token);
    }
    pdp_critical(builder.ToSlice());
  }

 private:
  struct TableEntry {
    Handle coro;
    uint32_t token;
  };

  LoopQueue<TableEntry> table;
};

}  // namespace pdp

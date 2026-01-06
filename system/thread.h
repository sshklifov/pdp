#pragma once

#include <pthread.h>
#include <utility>
#include "data/allocator.h"

namespace pdp {

template <typename T, typename... Rest>
struct Pack : Pack<Rest...> {
  template <typename _T1, typename... _T2>
  Pack(_T1 &&head, _T2 &&...args)
      : Pack<Rest...>(std::forward<_T2>(args)...), head(std::forward<_T1>(head)) {}

  T &GetHead() { return head; }
  Pack<Rest...> &GetTail() { return (*this); }

 private:
  T head;
};

struct EmptyPack {};

template <typename T>
struct Pack<T> : public EmptyPack {
  template <typename _T>
  Pack(_T &&head) : leaf(std::forward<_T>(head)) {}

  T &GetHead() { return leaf; }

  EmptyPack &GetTail() { return (*this); }

 private:
  T leaf;
};

template <typename Fun, typename... Pargs, typename... Eargs>
static void ExpandAndInvoke(Fun &f, Pack<Pargs...> &pack, Eargs &...exp) {
  if constexpr (sizeof...(Pargs) > 1) {
    return ExpandAndInvoke(f, pack.GetTail(), exp..., pack.GetHead());
  } else {
    return f(exp..., pack.GetHead());
  }
}

template <typename Fun, typename... Args>
static void Invoke(Fun &f, Pack<Args...> &pack) {
  ExpandAndInvoke(f, pack.GetTail(), pack.GetHead());
}

template <typename Fun, typename... Args>
struct PthreadData {
  template <typename _F, typename... _A>
  PthreadData(_F &&f, _A &&...a) : fun(std::forward<_F>(f)), args(std::forward<_A>(a)...) {}

  Fun fun;
  Pack<Args...> args;
};

template <typename Fun, typename... Args>
static void *ThreadEntry(void *user) {
  using Data = PthreadData<Fun, Args...>;
  Data *p = (Data *)user;
  Invoke(p->fun, p->args);
  free(p);
  return NULL;
}

struct Thread {
  Thread() : is_joinable(false) {}

  template <typename Fun, typename... Args>
  void Start(Fun &&f, Args &&...args) {
    static_assert(std::is_invocable_v<std::decay_t<Fun>, std::decay_t<Args>...>,
                  "Cannot invoke function with supplied arguments");

    pdp_assert(!is_joinable);

    using Data = PthreadData<std::decay_t<Fun>, std::decay_t<Args>...>;
    Data *p = (Data *)allocator.AllocateRaw(sizeof(Data));
    new (p) Data(std::forward<Fun>(f), std::forward<Args>(args)...);

    int s = pthread_create(&thread, NULL, ThreadEntry<std::decay_t<Fun>, std::decay_t<Args>...>, p);
    if (PDP_LIKELY(s == 0)) {
      is_joinable = true;
    } else {
      // 'p' will leak if not aborted.
      PDP_UNREACHABLE();
    }
  }

  void Wait() { pthread_join(thread, NULL); }

 private:
  pthread_t thread;
  bool is_joinable;
  DefaultAllocator allocator;
};

struct StoppableThread {
  StoppableThread() : is_running(false) {}

  template <typename Fun, typename... Args>
  void Start(Fun &&f, Args &&...args) {
    static_assert(std::is_invocable_v<std::decay_t<Fun>, std::atomic_bool *, std::decay_t<Args>...>,
                  "Thread function must accept std::atomic_bool* as first argument");
    pdp_assert(!is_running.load());

    using Data = PthreadData<std::decay_t<Fun>, std::atomic_bool *, std::decay_t<Args>...>;
    Data *p = (Data *)allocator.AllocateRaw(sizeof(Data));
    new (p) Data(std::forward<Fun>(f), &is_running, std::forward<Args>(args)...);

    is_running.store(true);
    int s = pthread_create(
        &thread, NULL, ThreadEntry<std::decay_t<Fun>, std::atomic_bool *, std::decay_t<Args>...>,
        p);

    // 'p' will leak if not aborted. Also 'is_running' will store the wrong value.
    if (PDP_UNLIKELY(s != 0)) {
      PDP_UNREACHABLE();
    }
  }

  void Wait() {
    is_running.store(false);
    pthread_join(thread, NULL);
  }

 private:
  pthread_t thread;
  std::atomic_bool is_running;
  DefaultAllocator allocator;
};

};  // namespace pdp

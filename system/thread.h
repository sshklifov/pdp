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

template <typename Fun, typename T, typename... Args>
static void *ThreadEntry(void *user) {
  using Data = PthreadData<Fun, T, Args...>;
  Data *p = (Data *)user;
  Invoke(p->fun, p->args);
  free(p);
  return NULL;
}

template <typename Fun>
static void *ThreadEntry(void *user) {
  Fun *p = (Fun *)user;
  (*p)();
  free(p);
  return NULL;
}

// TODO: add template of arguments?
struct Thread {
  Thread() : is_joinable(false) {}

  ~Thread() { pdp_assert(!is_joinable); }

  template <typename Fun, typename... Args>
  void Start(Fun &&f, Args &&...args) {
    static_assert(std::is_invocable_v<std::decay_t<Fun>, std::decay_t<Args>...>,
                  "Cannot invoke function with supplied arguments");

    pdp_assert(!is_joinable);

    using Data = std::conditional_t<(sizeof...(Args) > 0),
                                    PthreadData<std::decay_t<Fun>, std::decay_t<Args>...>,
                                    std::decay_t<Fun>>;
    Data *p = (Data *)allocator.AllocateRaw(sizeof(Data));
    new (p) Data(std::forward<Fun>(f), std::forward<Args>(args)...);

    int s = pthread_create(&thread, NULL, ThreadEntry<std::decay_t<Fun>, std::decay_t<Args>...>, p);
    if (PDP_LIKELY(s == 0)) {
#ifdef PDP_ENABLE_ASSERT
      is_joinable = true;
#endif
    } else {
      // 'p' will leak if not aborted.
      PDP_UNREACHABLE("Spawn thread failed");
    }
  }

  void Wait() {
    pdp_assert(is_joinable);
    pthread_join(thread, NULL);
    is_joinable = false;
  }

 private:
  pthread_t thread;
#if PDP_ENABLE_ASSERT
  bool is_joinable;
#endif
  DefaultAllocator allocator;
};

struct StoppableThread {
  StoppableThread() : is_running(false) {}

  template <typename Fun, typename... Args>
  void Start(Fun &&f, Args &&...args) {
    pdp_assert(!is_running.load());
    if (!is_running.exchange(true)) {
      thread.Start(f, &is_running, std::forward<Args>(args)...);
    }
  }

  void Stop() {
    if (is_running.exchange(false)) {
      thread.Wait();
    }
  }

 private:
  std::atomic_bool is_running;
  Thread thread;
};

};  // namespace pdp

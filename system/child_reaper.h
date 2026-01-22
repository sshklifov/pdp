#pragma once

#include "data/allocator.h"
#include "strings/string_slice.h"

#include <sys/types.h>
#include <csignal>
#include <cstddef>

namespace pdp {

inline StringSlice GetSignalDescription(int signal) { return StringSlice(sigabbrev_np(signal)); }

struct ChildReaper {
  using OnReapedChild = void (*)(pid_t, int, void *);

  ChildReaper();
  ~ChildReaper();

  void OnChildExited(pid_t pid, OnReapedChild cb, void *user_data);
  void Reap();
  void ReapAll();

 private:
  void WaitPid(int option);

  static void OnAnyChildExited(int);

  static constexpr size_t max_children = 16;

  struct ChildRegistry {
    pid_t pid;
    OnReapedChild reap_handler;
    void *user_data;
  };

  ChildRegistry *registry;
  unsigned num_children;

  static sig_atomic_t can_wait_flag;

  DefaultAllocator allocator;
};

};  // namespace pdp

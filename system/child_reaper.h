#pragma once

#include "data/allocator.h"

#include <sys/types.h>
#include <cstddef>

namespace pdp {

struct ChildReaper {
  using OnReapedChild = void (*)(void *, pid_t, int);

  ChildReaper();
  ~ChildReaper();

  void OnChildExited(void *user_data, pid_t pid, OnReapedChild cb);
  void Reap();
  void ReapAll();

 private:
  void ReapWithOption(int option);

  static constexpr size_t max_children = 16;

  struct ChildRegistry {
    pid_t pid;
    OnReapedChild reap_handler;
    void *user_data;
  };

  ChildRegistry *registry;
  unsigned num_children;

  DefaultAllocator allocator;
};

};  // namespace pdp

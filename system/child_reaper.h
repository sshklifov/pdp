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

  void WatchChild(pid_t pid, OnReapedChild cb, void *user_data);
  void *UnwatchChild(pid_t pid);

  void Reap();
  void ReapAll();

  static void PrintStatus(pid_t pid, int status);
  static void PrintStatus(const StringSlice &pretty_name, int status);

 private:
  void WaitPid(int option);

  static void DefaultHandler(pid_t pid, int status, void *);

  static void OnSigChild(int);

  static constexpr size_t max_children = 16;

  struct ChildRegistry {
    pid_t pid;
    OnReapedChild reap_handler;
    void *user_data;
  };

  ChildRegistry *registry;
  unsigned num_children;

  static sig_atomic_t has_more_children;

  DefaultAllocator allocator;
};

};  // namespace pdp

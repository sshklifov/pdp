#include "child_reaper.h"
#include "core/check.h"

#include <sys/wait.h>
#include <cstring>

namespace pdp {

sig_atomic_t ChildReaper::can_wait_flag = 0;

void ChildReaper::OnAnyChildExited(int) { can_wait_flag = 1; }

ChildReaper::ChildReaper() {
  num_children = 0;
  registry = Allocate<ChildRegistry>(allocator, max_children);
  memset(registry, -1, sizeof(ChildRegistry) * max_children);

  struct sigaction sa;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = SA_NOCLDSTOP | SA_RESTART;
  sa.sa_handler = OnAnyChildExited;
  CheckFatal(sigaction(SIGCHLD, &sa, NULL), "ChildReaper: sigaction");
}

ChildReaper::~ChildReaper() {
  pdp_assert(num_children == 0);
  Deallocate<ChildRegistry>(allocator, registry);
}

void ChildReaper::OnChildExited(pid_t pid, OnReapedChild cb, void *user_data) {
  for (size_t i = 0; i < max_children; ++i) {
    if (registry[i].pid < 0) {
      registry[i].pid = pid;
      registry[i].reap_handler = cb;
      registry[i].user_data = user_data;
      ++num_children;
      return;
    }
  }
  PDP_UNREACHABLE("ChildReaper: too many children registered!");
}

void ChildReaper::Reap() {
  if (PDP_UNLIKELY(can_wait_flag)) {
    WaitPid(WNOHANG);
  }
}

void ChildReaper::ReapAll() {
  const int wait_indefinitely = 0;
  while (PDP_UNLIKELY(num_children > 0)) {
    WaitPid(wait_indefinitely);
  }
}

void ChildReaper::WaitPid(int option) {
  if (PDP_LIKELY(num_children <= 0)) {
    return;
  }

  int status = 0;
  pid_t pid = waitpid(-1, &status, option);
  if (pid > 0) {
    for (size_t i = 0; i < max_children; ++i) {
      if (pid == registry[i].pid) {
        registry[i].pid = -1;
        --num_children;
        auto cb = registry[i].reap_handler;
        return cb(pid, status, registry[i].user_data);
      }
    }
    PDP_UNREACHABLE("ChildReaper: unhandled child received by waitpid!");
  } else if (pid != 0) {
    Check(pid, "waitpid");
  }
}

}  // namespace pdp

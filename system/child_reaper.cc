#include "child_reaper.h"
#include "core/check.h"

#include <sys/wait.h>
#include <cstring>

namespace pdp {

ChildReaper::ChildReaper() {
  num_children = 0;
  registry = Allocate<ChildRegistry>(allocator, max_children);
  memset(registry, -1, sizeof(ChildRegistry) * max_children);
}

ChildReaper::~ChildReaper() {
  pdp_assert(num_children == 0);
  Deallocate<ChildRegistry>(allocator, registry);
}

void ChildReaper::OnChildExited(void *user_data, pid_t pid, OnReapedChild cb) {
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

void ChildReaper::Reap() { ReapWithOption(WNOHANG); }

void ChildReaper::ReapAll() {
  while (PDP_UNLIKELY(num_children > 0)) {
    ReapWithOption(0);
  }
}

void ChildReaper::ReapWithOption(int option) {
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
        return cb(registry[i].user_data, pid, status);
      }
    }
    PDP_UNREACHABLE("ChildReaper: unhandled child received by waitpid!");
  } else if (pid != 0) {
    Check(pid, "waitpid");
  }
}

}  // namespace pdp

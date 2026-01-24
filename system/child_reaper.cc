#include "child_reaper.h"
#include "core/check.h"
#include "core/log.h"
#include "tracing/execution_tracer.h"

#include <sys/wait.h>
#include <cstring>

namespace pdp {

sig_atomic_t ChildReaper::has_more_children = 0;

void ChildReaper::OnSigChild(int) { has_more_children = 1; }

void ChildReaper::DefaultHandler(pid_t pid, int status, void *) {
  ChildReaper::PrintStatus(pid, status);
}

ChildReaper::ChildReaper() {
  num_children = 0;
  registry = Allocate<ChildRegistry>(allocator, max_children);
  memset(registry, -1, sizeof(ChildRegistry) * max_children);

  struct sigaction sa;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = SA_NOCLDSTOP | SA_RESTART;
  sa.sa_handler = OnSigChild;
  CheckFatal(sigaction(SIGCHLD, &sa, NULL), "ChildReaper: sigaction");
}

ChildReaper::~ChildReaper() {
  ReapAll();
  Deallocate<ChildRegistry>(allocator, registry);
}

void ChildReaper::WatchChild(pid_t pid, OnReapedChild cb, void *user_data) {
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

void *ChildReaper::UnwatchChild(pid_t pid) {
  for (size_t i = 0; i < max_children; ++i) {
    if (registry[i].pid == pid) {
      registry[i].pid = pid;
      registry[i].reap_handler = DefaultHandler;
      void *ud = registry[i].user_data;
      registry[i].user_data = nullptr;
      return ud;
    }
  }
  PDP_UNREACHABLE("ChildReaper: failed to find watch!");
}

void ChildReaper::PrintStatus(pid_t pid, int status) {
  char str[EstimateSizeV<pid_t>];
  Formatter fmt(str);
  fmt.AppendUnchecked(pid);
  PrintStatus(StringSlice(str, fmt.End()), status);
}

void ChildReaper::PrintStatus(const StringSlice &name, int status) {
  if (WIFSIGNALED(status)) {
    int sig = WTERMSIG(status);
    pdp_warning("Child {} terminated by signal {}", name, GetSignalDescription(sig));
  } else if (WIFEXITED(status)) {
    int exit_status = WEXITSTATUS(status);
    if (exit_status != 0) {
      pdp_warning("Child {} exited with code {}", name, exit_status);
    } else {
      pdp_info("Child {} exited normally", name);
    }
  } else {
    pdp_warning("Child {} unknown termination state");
  }
}

void ChildReaper::Reap() {
  const bool enable_optimization = g_recorder.IsNormal();
  if (PDP_UNLIKELY(enable_optimization && has_more_children)) {
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
  pid_t pid = g_recorder.SyscallWaitPid(&status, option);
  has_more_children = (pid > 0);
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

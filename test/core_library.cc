#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "core/check.h"
#include "core/log.h"
#include "tracing/trace_likely.h"

static void RunChildAndTerminate(pdp::StringSlice name, void (*callback)()) {
  pid_t pid = fork();
  if (pid == 0) {
    // Child
    pdp_info("Child [{}]: about to die", name);
    callback();

    // If we get here, the test FAILED.
    pdp_critical("Child [{}]: survived (BUG)", name);
    _exit(0);
  }

  // Parent
  int status = 0;
  waitpid(pid, &status, 0);

  if (WIFSIGNALED(status)) {
    int sig = WTERMSIG(status);
    pdp_info("Parent: [{}] terminated by signal {}", name, sig);

    // Most asserts abort via SIGABRT
    if (sig != SIGABRT) {
      pdp_warning("Parent: unexpected signal for {}", name);
    }
  } else if (WIFEXITED(status)) {
    pdp_warning("Parent: [{] exited normally with code {} (BUG)", name, WEXITSTATUS(status));
  } else {
    pdp_warning("Parent: [{}] unknown termination state", name);
  }
}

static void TestAssert() { pdp_assert(false && "Intentional pdp_assert test"); }

static void TestCheckAndTerminate() {
  int ret = close(-1);
  pdp::CheckAndTerminate(ret, "try to close() with invalid file descriptor");
}

int main() {
  pdp_info("=== Core library test ===");

  pdp_info("Testing logging");
  const pdp::Level levels[] = {pdp::Level::kInfo, pdp::Level::kWarn, pdp::Level::kError};
  for (auto level : levels) {
    pdp_critical("Changing log level");
    pdp::SetConsoleLogLevel(level);
    pdp_info("info message");
    pdp_warning("warn message");
    pdp_error("error message");
  }

  pdp::SetConsoleLogLevel(pdp::Level::kInfo);
  pdp_info("Restogin logging level");

  pdp_info("Testing PDP_TRACE_LIKELY / PDP_TRACE_UNLIKELY with mispredictions.");
  const size_t mispredictions = 4;
  for (size_t i = 0; i < mispredictions; ++i) {
    if (PDP_TRACE_LIKELY(i * i == i + i)) {
      pdp_info("likely branch taken");
    }
  }
  for (size_t i = 0; i < mispredictions; ++i) {
    if (PDP_TRACE_UNLIKELY(i % 2 == 0)) {
      pdp_info("unlikely branch taken");
    }
  }

  pdp_info("Testing pdp_assert death");
  RunChildAndTerminate("pdp_assert", TestAssert);

  pdp_info("Testing CheckAndTerminate death");
  RunChildAndTerminate("CheckAndTerminate", TestCheckAndTerminate);

  pdp_info("=== Core library test END ===");
  return 0;
}

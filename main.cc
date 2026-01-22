#include "core/log.h"
#include "coroutines/debug_coordinator.h"
#include "system/poll_table.h"

#include <sys/prctl.h>

using pdp::operator""_ms;

int main() {
#ifdef PDP_DEBUG_BUILD
  prctl(PR_SET_PTRACER, PR_SET_PTRACER_ANY);
#endif

  pdp::RedirectLogging("/home/stef/Downloads/output.txt");

  pdp_info("Setting up SIGCHLD handler");
  pdp::ChildReaper reaper;

  pdp_info("Starting coordinator");
  pdp::StringSlice host("");
  pdp::DebugCoordinator coordinator(host, pdp::DuplicateForThisProcess(STDOUT_FILENO),
                                    pdp::DuplicateForThisProcess(STDIN_FILENO), reaper);

  pdp::PollTable poller;
  pdp_info("Polling until idle state is reached");

  pdp::Stopwatch stopwatch;
  while (!coordinator.IsIdle() && stopwatch.Elapsed() < 5000_ms) {
    // Poll file descriptors
    coordinator.RegisterForPoll(poller);
    poller.Poll(pdp::Milliseconds(100));
    coordinator.OnPollResults(poller);
    poller.Reset();
    // Check for exited children.
    reaper.Reap();
  }

  pdp_info("Done! Exitting cleanly...");

  return 0;
}

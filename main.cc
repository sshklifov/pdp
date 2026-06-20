#include "core/log.h"
#include "coroutines/debug_coordinator.h"
#include "system/poll_table.h"

#include <sys/prctl.h>

using pdp::operator""_ms;
using pdp::g_recorder;

void ApplicationMain() {
  pdp_info("Setting up SIGCHLD handler");
  pdp::ChildReaper reaper;

  pdp_info("Starting coordinator");
  pdp::StringSlice host("");
  pdp::DebugCoordinator coordinator(host, pdp::DuplicateForThisProcess(STDOUT_FILENO),
                                    pdp::DuplicateForThisProcess(STDIN_FILENO), reaper);

  pdp::PollTable poller;
  pdp_info("Polling until idle state is reached");

  // TODO test breakpoints. it at least prints something.
  coordinator.GdbDriver().Send("-break-insert main");

  pdp::Stopwatch stopwatch;
  while (g_recorder.IsTimeLess(stopwatch.Elapsed(), 5000_ms)) {
    // Poll file descriptors
    poller.Reset();
    coordinator.RegisterForPoll(poller);
    poller.Poll(pdp::Milliseconds(100));
    coordinator.OnPollResults(poller);
    // Check for exited children.
    reaper.Reap();
  }
  pdp_info("Done! Exitting ApplicationMain()...");
}

int main(int argc, char **argv) {
#ifdef PDP_DEBUG_BUILD
  prctl(PR_SET_PTRACER, PR_SET_PTRACER_ANY);
#endif

  if (argc > 2 && pdp::StringSlice(argv[2]) == "--output") {
    pdp::RedirectLogging(pdp::DuplicateForThisProcess(STDOUT_FILENO));
  } else {
    pdp::RedirectLogging(PDP_LOG_PATH);
  }

  if (argc > 1 && pdp::StringSlice(argv[1]) == "--replay") {
    g_recorder.StartReplaying();
  } else if (argc > 1 && pdp::StringSlice(argv[1]) == "--record") {
    g_recorder.StartRecording();
  }

  ApplicationMain();

  g_recorder.CheckForEndOfStream();
  g_recorder.Stop();
  return 0;
}

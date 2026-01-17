#include "core/log.h"
#include "handlers/debug_coordinator.h"

#include <sys/prctl.h>

int main() {
#ifdef PDP_DEBUG_BUILD
  prctl(PR_SET_PTRACER, PR_SET_PTRACER_ANY);
#endif

  pdp::RedirectLogging("/home/stef/Downloads/output.txt");

  pdp_info("Starting coordinator");
  pdp::DebugCoordinator coordinator(pdp::DuplicateForThisProcess(STDOUT_FILENO),
                                    pdp::DuplicateForThisProcess(STDIN_FILENO));
  coordinator.ReachIdle(pdp::Milliseconds(5000));

  while (true) {
    coordinator.PollGdb(pdp::Milliseconds(100));
    coordinator.PollVim(pdp::Milliseconds(100));
  }

  return 0;
}

#include "core/log.h"

#include <sys/prctl.h>

int main() {
#ifdef PDP_DEBUG_BUILD
  prctl(PR_SET_PTRACER, PR_SET_PTRACER_ANY);
#endif

  pdp::SetConsoleLogLevel(pdp::Level::kInfo);

  return 0;
}

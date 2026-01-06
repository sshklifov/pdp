#include "application/gdb_driver.h"
#include "core/log.h"

#include <sys/prctl.h>

int main() {
#ifdef PDP_DEBUG_BUILD
  prctl(PR_SET_PTRACER, PR_SET_PTRACER_ANY);
#endif

  pdp::SetConsoleLogLevel(pdp::Level::kInfo);

  pdp::GdbDriver driver;
  driver.Start();
  driver.Request("-exec-run --start");
  while (true) {
    driver.Poll(pdp::Milliseconds(1000));
  }

  return 0;
}

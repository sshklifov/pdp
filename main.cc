#include "application/gdb_driver.h"
#include "core/log.h"

int main() {
  pdp::SetConsoleLogLevel(pdp::Level::kInfo);

  pdp::GdbDriver driver;
  driver.Start();
  driver.Request("-exec-run --start");
  while (true) {
    driver.Poll(pdp::Milliseconds(1000));
  }

  return 0;
}

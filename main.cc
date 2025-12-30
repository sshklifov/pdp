#include "core/log.h"
#include "gdb_session.h"

void DefaultStreamCallback(const pdp::StringSlice &s) { pdp_info(s); }

void DefaultAsyncCallback(const pdp::StringSlice &s) { pdp_info(s); }

int main() {
  pdp::SetConsoleLogLevel(pdp::Level::kInfo);

  pdp::GdbSession session(DefaultAsyncCallback, DefaultStreamCallback);
  session.Start();
  session.SendCommand("-exec-run --start", nullptr);
  while (true) {
    session.Poll(std::chrono::seconds(1));
  }

  return 0;
}

#include "gdb_session.h"
#include "log.h"

void DefaultStreamCallback(const pdp::StringView &s) { pdp_info(s); }

void DefaultAsyncCallback(const pdp::StringView &s) { pdp_info(s); }

int main() {
#ifdef PDP_ENABLE_TRACE
  pdp::SetConsoleLogLevel(pdp::Level::kTrace);
#else
  pdp::SetConsoleLogLevel(pdp::Level::kInfo);
#endif

  pdp::GdbSession session(DefaultAsyncCallback, DefaultStreamCallback);
  session.Start();
  // session.SendCommand("-gdb-version", nullptr);
  session.Poll(std::chrono::seconds(1));

  return 0;
}

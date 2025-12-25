#include "gdb_session.h"

int main() {
  pdp::GdbSession session;
  session.Start();
  session.SendCommand("-gdb-version", nullptr);
  session.Poll(std::chrono::seconds(1));

  return 0;
}

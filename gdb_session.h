#pragma once

#include "rolling_buffer.h"

#include <chrono>
#include <functional>

namespace pdp {

struct GdbSession {
  using Callback = std::function<void(const StringView &)>;

  GdbSession();
  ~GdbSession();

  void Start();

  void SendCommand(const StringView &command, Callback cb);

  void Poll(std::chrono::milliseconds ms);

 private:
  void Process();

  int in[2];   // parent -> gdb (stdin)
  int out[2];  // gdb -> parent (stdout)
  int err[2];  // gdb -> parent (stderr)

  unsigned token_counter;

  std::vector<Callback> callbacks;

  bool disconnected;
  RollingBuffer buffer;
};

};  // namespace pdp

#pragma once

#include "rolling_buffer.h"

#include <chrono>
#include <functional>

namespace pdp {

struct GdbSession {
  using Callback = std::function<void(const StringSlice &)>;

  GdbSession(Callback async_callback, Callback stream_callback);
  ~GdbSession();

  void Start();

  void SendCommand(const StringSlice &command, Callback cb);

  void Poll(std::chrono::milliseconds ms);

 private:
  void Process();

  int in[2];   // parent -> gdb (stdin)
  int out[2];  // gdb -> parent (stdout)
  int err[2];  // gdb -> parent (stderr)

  unsigned token_counter;

  std::vector<Callback> callbacks;
  Callback async_callback;
  Callback stream_callback;

  bool disconnected;
  RollingBuffer buffer;
};

};  // namespace pdp

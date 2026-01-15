#pragma once

namespace pdp {

struct DebugSession {
  int selected_thread;
  int is_stopped;
  int source_bufnr;
};

}  // namespace pdp

#pragma once

#include "core/check.h"

#include <cstdint>

namespace pdp {

enum VimNamespaces {
  kHighlightNs,
  kProgramCounterNs,
  kRegisterNs,
  kPromptBufferNs,
  kConcealVarNs,
  kConcealJumpNs,
  kBreakpointNs,
  kTotalNs
};

enum VimBuffers { kCaptureBuf, kAsmBuf, kPromptBuf, kIoBuf, kTotalBufs };

struct DebugSession {
  DebugSession() {
    selected_thread = 0;
    selected_frame = 0;
    is_stopped = 1;
    asm_mode = 0;
    exe_timestamp = 0;

    pid = -1;
  }

  bool HasExeTimestamp() const { return exe_timestamp > 0; }

  void SetExeTimestamp(int64_t ts) {
    pdp_assert(!HasExeTimestamp());
    exe_timestamp = ts;
  }

  int64_t GetExeTimestamp() const { return exe_timestamp; }

  int selected_thread;
  int selected_frame;
  int is_stopped;
  int asm_mode;

  int pid;

  int source_bufnr;
  int num_lines_written;

  int namespaces[kTotalNs];
  int buffers[kTotalBufs];

 private:
  int64_t exe_timestamp;
};

}  // namespace pdp

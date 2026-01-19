#pragma once

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
  int selected_thread;
  int is_stopped;
  int source_bufnr;
  int num_lines_written;

  int namespaces[kTotalNs];
  int buffers[kTotalBufs];
};

}  // namespace pdp

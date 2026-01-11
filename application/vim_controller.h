#pragma once

#include "strings/byte_stream.h"
#include "strings/string_slice.h"

#include <initializer_list>

namespace pdp {

struct VimController {
  VimController(int input_fd, int output_fd);

  void ShowNormal(const StringSlice &msg);
  void ShowWarning(const StringSlice &msg);
  void ShowError(const StringSlice &msg);
  void ShowMessage(const StringSlice &msg, const StringSlice &hl);
  void ShowMessage(std::initializer_list<StringSlice> msg, std::initializer_list<StringSlice> hl);

  bool Poll(Milliseconds timeout);

  bool SendRpc(const void *bytes, size_t num_bytes);

 private:
  OutputDescriptor vim_input;
  ByteStream vim_output;
};

}  // namespace pdp

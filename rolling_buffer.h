#pragma once

#include "string_view.h"

namespace pdp {

struct RollingBuffer {
  static constexpr const size_t default_buffer_capacity = 16384;

  RollingBuffer();
  ~RollingBuffer();

  size_t ReadFull(int fd);
  size_t ReadOnce(int fd);

  StringView ConsumeLine();

  StringView ViewOnly() const;

  bool Empty() const;

 private:
  StringView ConsumeChars(size_t n);

  void Relocate();  // TODO rename
  void GrowExtra();

  char *ptr;
  size_t num_skipped;
  size_t num_read;
  size_t num_free;
};

}  // namespace pdp

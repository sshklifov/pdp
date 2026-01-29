#pragma once

#include "strings/fixed_string.h"

namespace pdp {

struct JumpLocation {
  JumpLocation(int k, int l, FixedString &&f) : key(k), jump_line(l), jump_file(std::move(f)) {}

  int key;
  int jump_line;
  FixedString jump_file;
};

template <>
struct CanReallocate<JumpLocation> : std::true_type {};

struct JumpTable {
  void Insert(int key, const StringSlice &fullname, int lnum) {
    pdp_assert(jumps.Empty() || jumps.Last().key < key);
    jumps.Emplace(key, lnum, FixedString(fullname));
  }

  JumpLocation &Find(int key) {
    size_t lo = 0;
    size_t hi = jumps.Size();
    while (hi - lo > 1) {
      size_t mid = (lo + hi) / 2;
      if (key < jumps[mid].key) {
        hi = mid;
      } else {
        lo = mid;
      }
    }
    pdp_assert(jumps[lo].key == key);
    return jumps[lo];
  }

 private:
  pdp::Vector<JumpLocation> jumps;
};

}  // namespace pdp

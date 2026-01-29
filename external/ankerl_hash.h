///////////////////////// ankerl::unordered_dense::{map, set} /////////////////////////

// A fast & densely stored hashmap and hashset based on robin-hood backward shift deletion.
// Version 1.3.0
// https://github.com/martinus/unordered_dense
//
// Licensed under the MIT License <http://opensource.org/licenses/MIT>.
// SPDX-License-Identifier: MIT
// Copyright (c) 2022 Martin Leitner-Ankerl <martin.ankerl@gmail.com>
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#pragma once

#include "core/internals.h"
#include "strings/fixed_string.h"
#include "strings/string_slice.h"

#include <cstdint>
#include <cstring>

namespace ankerl {

namespace unordered_dense {

static inline void mum(uint64_t *a, uint64_t *b) {
#if defined(__SIZEOF_INT128__)
  __uint128_t r = *a;
  r *= *b;
  *a = static_cast<uint64_t>(r);
  *b = static_cast<uint64_t>(r >> 64U);
#else
#error No 128-bit support on target machine.
#endif
}

[[nodiscard]]
static inline uint64_t mix(uint64_t a, uint64_t b) {
  mum(&a, &b);
  return a ^ b;
}

[[nodiscard]]
static inline uint64_t hash(uint64_t x) {
  return mix(x, UINT64_C(0x9E3779B97F4A7C15));
}

// read functions. WARNING: we don't care about endianness, so results are different on big endian!
[[nodiscard]]
static inline uint64_t r8(const uint8_t *p) {
  uint64_t v;
  memcpy(&v, p, 8);
  return v;
}

[[nodiscard]]
static inline uint64_t r4(const uint8_t *p) {
  uint32_t v;
  memcpy(&v, p, 4);
  return v;
}

// reads 1, 2, or 3 bytes
[[nodiscard]]
static inline uint64_t r3(const uint8_t *p, size_t k) {
  return (static_cast<uint64_t>(p[0]) << 16U) | (static_cast<uint64_t>(p[k >> 1U]) << 8U) |
         p[k - 1];
}

#if 1
[[nodiscard]]
static inline uint32_t hash(const void *key, size_t len) {
  uint64_t secret[] = {UINT32_C(0xa0761d6478bd642f), UINT32_C(0xe7037ed1a0b428db),
                       UINT32_C(0x8ebc6af09c88c6e3), UINT32_C(0x589965cc75374cc3)};

  const uint8_t *p = static_cast<const uint8_t *>(key);
  uint64_t seed = secret[0];
  uint64_t a;
  uint64_t b;
  if (PDP_LIKELY(len <= 16)) {
    if (PDP_LIKELY(len >= 4)) {
      a = (r4(p) << 32U) | r4(p + ((len >> 3U) << 2U));
      b = (r4(p + len - 4) << 32U) | r4(p + len - 4 - ((len >> 3U) << 2U));
    } else if (PDP_LIKELY(len > 0)) {
      a = r3(p, len);
      b = 0;
    } else {
      a = 0;
      b = 0;
    }
  } else {
    size_t i = len;
    if (PDP_UNLIKELY(i > 48)) {
      uint64_t see1 = seed;
      uint64_t see2 = seed;
      do {
        seed = mix(r8(p) ^ secret[1], r8(p + 8) ^ seed);
        see1 = mix(r8(p + 16) ^ secret[2], r8(p + 24) ^ see1);
        see2 = mix(r8(p + 32) ^ secret[3], r8(p + 40) ^ see2);
        p += 48;
        i -= 48;
      } while (PDP_LIKELY(i > 48));
      seed ^= see1 ^ see2;
    }
    while (PDP_UNLIKELY(i > 16)) {
      seed = mix(r8(p) ^ secret[1], r8(p + 8) ^ seed);
      i -= 16;
      p += 16;
    }
    a = r8(p + i - 16);
    b = r8(p + i - 8);
  }

  return mix(len ^ secret[1], mix(a ^ secret[1], b ^ seed));
}
#endif

}  // namespace unordered_dense

}  // namespace ankerl

namespace pdp {

template <typename T>
struct Hash;

template <typename T>
struct Hash<T *> {
  uint64_t operator()(T *ptr) const {
    return ankerl::unordered_dense::hash(reinterpret_cast<uint64_t>(ptr));
  }
};

template <>
struct Hash<StringSlice> {
  uint64_t operator()(const StringSlice &s) const {
    return ankerl::unordered_dense::hash(s.Begin(), s.Size());
  }
};

template <>
struct Hash<FixedString> {
  uint64_t operator()(const FixedString &str) const {
    return ankerl::unordered_dense::hash(str.Begin(), str.Size());
  }
};

template <>
struct Hash<uint64_t> {
  uint64_t operator()(uint32_t value) const { return ankerl::unordered_dense::hash(value); }
};

template <>
struct Hash<int64_t> {
  uint64_t operator()(uint32_t value) const { return ankerl::unordered_dense::hash(value); }
};

template <>
struct Hash<uint32_t> {
  uint64_t operator()(uint32_t value) const { return ankerl::unordered_dense::hash(value); }
};

template <>
struct Hash<int32_t> {
  uint64_t operator()(uint32_t value) const { return ankerl::unordered_dense::hash(value); }
};

}  // namespace pdp

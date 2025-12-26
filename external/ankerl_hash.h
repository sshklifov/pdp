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

#include "log.h"

#include <cstdint>

namespace ankerl {

namespace unordered_dense {

static inline void mum(uint64_t *a, uint64_t *b) {
#if defined(__SIZEOF_INT128__)
  __uint128_t r = *a;
  r *= *b;
  *a = static_cast<uint64_t>(r);
  *b = static_cast<uint64_t>(r >> 64U);
#else
#warning No 128-bit support on target machine. Hashing is suboptimal!
#endif
}

[[nodiscard]] static inline uint64_t mix(uint64_t a, uint64_t b) {
  mum(&a, &b);
  return a ^ b;
}

static inline uint64_t hash(uint64_t x) { return mix(x, UINT64_C(0x9E3779B97F4A7C15)); }

}  // namespace unordered_dense

}  // namespace ankerl

namespace pdp {

template <typename T>
struct Hash;

template <typename T>
struct Hash<T *> {
  uint64_t operator()(T *ptr) const {
#ifndef PDP_ENABLE_EXTERNAL_CODE
    pdp_error("Dynamic usage of Ankerl hash!");
    std::terminate();
#endif
    return ankerl::unordered_dense::hash(reinterpret_cast<uint64_t>(ptr));
  }
};

}  // namespace pdp

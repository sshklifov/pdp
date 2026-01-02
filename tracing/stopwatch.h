#pragma once

#include <cstdint>

namespace pdp {
/// @brief Hardware accelerated stopwatch with nanosecond precision
class HardwareStopwatch {
 public:
  /// @brief Starts the stopwatch
  ///
  /// Keep the implementation here so the call is inlined as a single instruction.
  HardwareStopwatch() : start(ReadTSC()) {}

  /// @brief Returns the elapsed clocks.
  ///
  /// Can be called multiple times.
  uint64_t LapClocks() {
    uint64_t old_start = start;
    start = ReadTSC();
    return start - old_start;
  }

 private:
  static inline uint64_t ReadTSC() {
    uint32_t lo, hi;
    asm volatile("rdtscp" : "=a"(lo), "=d"(hi)::"rcx");
    return (uint64_t(hi) << 32) | lo;
  }

  uint64_t start;
};

}  // namespace pdp

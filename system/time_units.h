#pragma once

// TODO: rename file

#include <cstdint>
#include <ctime>

namespace pdp {

struct Milliseconds {
  explicit constexpr Milliseconds(int64_t value) : value(value) {}

  constexpr int64_t GetMilli() const { return value; };

  bool operator<(const Milliseconds &other) const { return value < other.value; }
  bool operator>(const Milliseconds &other) const { return value > other.value; }

  bool operator<=(const Milliseconds &other) const { return value <= other.value; }
  bool operator>=(const Milliseconds &other) const { return value >= other.value; }

  bool operator==(const Milliseconds &other) const { return value == other.value; }
  bool operator!=(const Milliseconds &other) const { return value != other.value; }

  Milliseconds &operator-=(const Milliseconds &other) {
    value -= other.value;
    return *this;
  }

  Milliseconds &operator+=(const Milliseconds &other) {
    value += other.value;
    return *this;
  }

  Milliseconds operator-(const Milliseconds &other) const {
    return Milliseconds(value - other.value);
  }

  Milliseconds operator+(const Milliseconds &other) const {
    return Milliseconds(value + other.value);
  }

 private:
  int64_t value;
};

struct Stopwatch {
  Stopwatch() { Reset(); }

  void Reset() {
    clock_gettime(CLOCK_MONOTONIC, &last_checkpoint);
  }

  Milliseconds ElapsedMilli() const {
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);

    int64_t elapsed_ms = (now.tv_sec - last_checkpoint.tv_sec) * 1'000 +
                         (now.tv_nsec - last_checkpoint.tv_nsec) / 1'000'000;
    return Milliseconds(elapsed_ms);
  }

 private:
  struct timespec last_checkpoint;
};

// @brief Gigabytes (GB)
constexpr Milliseconds operator""_ms(unsigned long long time) { return Milliseconds(time); }

};  // namespace pdp

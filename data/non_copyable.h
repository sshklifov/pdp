#pragma once

namespace pdp {

struct NonCopyable {
  NonCopyable() = default;
  ~NonCopyable() = default;

  NonCopyable(const NonCopyable &) = delete;
  NonCopyable &operator=(const NonCopyable &) = delete;
};

struct NonMoveable {
  NonMoveable() = default;
  ~NonMoveable() = default;

  NonMoveable(NonMoveable &&) = delete;
  NonMoveable &operator=(NonMoveable &&) = delete;
};

struct NonCopyableNonMovable {
  NonCopyableNonMovable() = default;
  ~NonCopyableNonMovable() = default;

  NonCopyableNonMovable(const NonCopyableNonMovable &) = delete;
  NonCopyableNonMovable &operator=(const NonCopyableNonMovable &) = delete;

  NonCopyableNonMovable(NonCopyableNonMovable &&) = delete;
  NonCopyableNonMovable &operator=(NonCopyableNonMovable &&) = delete;
};

}  // namespace pdp

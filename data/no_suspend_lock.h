#pragma once

#include "core/check.h"
#include "data/non_copyable.h"

namespace pdp {

struct NoSuspendLock {
  static void Lock() { ++depth; }

  static void Unlock() {
    pdp_assert(depth > 0);
    --depth;
  }

  static void CheckLocked() { pdp_assert(depth > 0); }

  static void CheckUnlocked() { pdp_assert(depth == 0); }

 private:
  static int depth;
};

struct NoSuspendGuard : public NonMoveable {
  NoSuspendGuard() { NoSuspendLock::Lock(); }

  NoSuspendGuard(const NoSuspendGuard &) { NoSuspendLock::Lock(); }

  void operator=(const NoSuspendGuard &rhs) = delete;

  ~NoSuspendGuard() { NoSuspendLock::Unlock(); }
};

template <typename WrapIt>
struct NoSuspendIterator : public NonMoveable {
  explicit NoSuspendIterator(const WrapIt &it) : it(it) {}

  NoSuspendIterator(const NoSuspendIterator &rhs) : it(rhs.it) {}

  NoSuspendIterator &operator=(const NoSuspendIterator &rhs) = delete;

  decltype(auto) operator*() { return *it; }

  decltype(auto) operator->() { return &(**this); }

  NoSuspendIterator &operator++() {
    ++it;
    return (*this);
  }

  bool operator==(const NoSuspendIterator &rhs) const { return it == rhs.it; }

  bool operator!=(const NoSuspendIterator &rhs) const { return it != rhs.it; }

 private:
  WrapIt it;
  NoSuspendGuard suspend_guard;
};

}  // namespace pdp

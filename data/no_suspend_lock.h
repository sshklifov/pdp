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

// TODO does this work? Integrate it if so.
template <typename T>
struct NoSuspendRef : public NonMoveable {
  NoSuspendRef(T &ref) : ref(ref) {
#ifdef PDP_ENABLE_ASSERT
    is_active = true;
#endif
    NoSuspendLock::Lock();
  }

  NoSuspendRef(const NoSuspendRef &rhs) : NoSuspendRef(rhs.ref) {}

  ~NoSuspendRef() {
    if (PDP_UNLIKELY(is_active)) {
      NoSuspendLock::Unlock();
    }
  }

  T &Get() {
    pdp_assert(is_active);
    return ref;
  }

  const T &Get() const {
    pdp_assert(is_active);
    return ref;
  }

  void Release() {
    if (PDP_LIKELY(is_active)) {
      is_active = false;
      NoSuspendLock::Unlock();
    }
  }

 private:
  T &ref;
#ifdef PDP_ENABLE_ASSERT
  bool is_active;
#endif
};

#if 0
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
#endif

}  // namespace pdp

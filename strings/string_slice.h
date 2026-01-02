#pragma once

#include "core/check.h"
#include "core/internals.h"

#include <cstddef>
#include <cstring>

namespace pdp {

inline constexpr size_t ConstexprLength(const char *s) {
  if (PDP_CONSTEXPR_EVALUATED()) {
    size_t size = 0;
    while (s[size] != '\0') {
      ++size;
    }
    return size;
  } else {
    return strlen(s);
  }
}

struct StringSlice {
  constexpr StringSlice(const char *p, size_t sz) : ptr(p), size(sz) {}

  constexpr StringSlice(const char *begin, const char *end) : ptr(begin), size(end - begin) {
    pdp_assert_non_constexpr(begin <= end);
  }

  constexpr StringSlice(const char *s) : ptr(s), size(ConstexprLength(s)) {}

  constexpr const char *Data() const { return ptr; }
  constexpr const char *Begin() const { return ptr; }
  constexpr const char *End() const { return ptr + size; }

  constexpr const char *Find(char c) const { return Find(Begin(), c); }

  constexpr const char *Find(const char *it, char c) const {
    pdp_assert_non_constexpr(it >= Begin() && it <= End());
    if (PDP_CONSTEXPR_EVALUATED()) {
      while (it < End() && *it != c) {
        ++it;
      }
      return it;
    } else {
      const char *ret = (const char *)memchr(it, c, End() - it);
      return ret != nullptr ? ret : End();
    }
  }

  constexpr const char *MemChar(char c) const {
    if (PDP_CONSTEXPR_EVALUATED()) {
      const char *it = Begin();
      while (it <= End() && *it != c) {
        ++it;
      }
      return it < End() ? it : nullptr;
    } else {
      return (const char *)memchr(Begin(), c, Size());
    }
  }

  constexpr const char *MemReverseChar(char c) const {
    if (PDP_CONSTEXPR_EVALUATED()) {
      const char *it = End() - 1;
      while (it >= Begin() && *it != c) {
        --it;
      }
      return it >= Begin() ? it : nullptr;
    } else {
      return (const char *)memrchr(Begin(), c, Size());
    }
  }

  constexpr StringSlice Substr(const char *it) const {
    pdp_assert_non_constexpr(it >= Begin() && it < End());
    return StringSlice(it, End());
  }

  constexpr StringSlice GetLeft(size_t n) {
    pdp_assert_non_constexpr(n <= Size());
    return StringSlice(ptr, n);
  }

  constexpr void DropLeft(const char *it) {
    pdp_assert_non_constexpr(it >= Begin() && it <= End());
    size -= it - ptr;
    ptr = it;
  }

  constexpr void DropLeft(size_t n) {
    pdp_assert_non_constexpr(n <= Size());
    ptr += n;
    size -= n;
  }

  constexpr bool StartsWith(char c) const { return !Empty() && *ptr == c; }

  constexpr bool Empty() const { return size == 0; }
  constexpr size_t Size() const { return size; }
  constexpr size_t Length() const { return size; }

  constexpr bool operator==(const StringSlice &other) const {
    if (Size() != other.Size()) {
      return false;
    }
    if (PDP_CONSTEXPR_EVALUATED()) {
      for (size_t i = 0; i < Size(); ++i) {
        if (ptr[i] != other[i]) {
          return false;
        }
      }
      return true;
    } else {
      return memcmp(Begin(), other.Begin(), Size()) == 0;
    }
  }

  constexpr bool operator!=(const StringSlice &other) const { return !(*this == other); }

  constexpr const char &operator[](size_t index) const { return ptr[index]; }

 private:
  const char *ptr;
  size_t size;
};

};  // namespace pdp

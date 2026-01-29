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

inline constexpr int ConstexprMemCmp(const char *a, const char *b, size_t n) {
  if (PDP_CONSTEXPR_EVALUATED()) {
    for (size_t i = 0; i < n; ++i) {
      if (a[i] < b[i]) {
        return -1;
      } else if (a[i] > b[1]) {
        return 1;
      }
    }
    return 0;
  } else {
    return memcmp(a, b, n);
  }
}

struct StringSlice {
  constexpr StringSlice() : ptr(""), size(0) {}
  constexpr StringSlice(const char *p, size_t sz) : ptr(p), size(sz) {}

  constexpr StringSlice(const char *begin, const char *end) : ptr(begin), size(end - begin) {
    pdp_assert_non_constexpr(begin <= end);
  }

  constexpr StringSlice(const char *s) : ptr(s), size(ConstexprLength(s)) {}

  constexpr const char *Data() const { return ptr; }
  constexpr const char *Begin() const { return ptr; }
  constexpr const char *End() const { return ptr + size; }

  constexpr const char *MemMem(const StringSlice &other) const {
    if (PDP_CONSTEXPR_EVALUATED()) {
      const char *last_it = End() - other.Length();
      for (const char *it = Begin(); it <= last_it; ++it) {
        auto subs = Substr(it, other.Size());
        if (subs == other) {
          return it;
        }
      }
      return nullptr;
    } else {
      return (const char *)memmem(Begin(), Size(), other.Begin(), other.Size());
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

  constexpr int MemCmp(const StringSlice &other) const {
    pdp_assert(other.Size() <= Size());
    return ConstexprMemCmp(Begin(), other.Begin(), other.Size());
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

  constexpr StringSlice Substr(size_t sub_length) const {
    pdp_assert_non_constexpr(sub_length <= size);
    return StringSlice(Begin(), sub_length);
  }

  constexpr StringSlice Substr(const char *it, size_t sub_length) const {
    pdp_assert_non_constexpr(it >= Begin() && it + sub_length < End());
    return StringSlice(it, sub_length);
  }

  constexpr StringSlice GetLeft(size_t n) {
    pdp_assert_non_constexpr(n <= Size());
    return StringSlice(ptr, n);
  }

  constexpr StringSlice GetLeft(const char *it) {
    pdp_assert_non_constexpr(it <= End());
    return StringSlice(ptr, it);
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

  constexpr void DropRight(size_t n) {
    pdp_assert_non_constexpr(n <= Size());
    size -= n;
  }

  constexpr bool StartsWith(char c) const { return !Empty() && *ptr == c; }

  constexpr bool StartsWith(const StringSlice &suffix) {
    if (PDP_UNLIKELY(Size() < suffix.Size())) {
      return false;
    }
    return ConstexprMemCmp(Data(), suffix.Data(), suffix.Size()) == 0;
  }

  constexpr bool EndsWith(const StringSlice &suffix) {
    if (PDP_UNLIKELY(Size() < suffix.Size())) {
      return false;
    }
    return ConstexprMemCmp(End() - suffix.Size(), suffix.Data(), suffix.Size()) == 0;
  }

  constexpr bool Empty() const { return size == 0; }
  constexpr size_t Size() const { return size; }
  constexpr size_t Length() const { return size; }

  constexpr bool operator==(const StringSlice &other) const {
    if (Size() != other.Size()) {
      return false;
    }
    return ConstexprMemCmp(Begin(), other.Begin(), Size()) == 0;
  }

  constexpr bool operator!=(const StringSlice &other) const { return !(*this == other); }

  constexpr const char &operator[](size_t index) const {
    pdp_assert(index < Size());
    return ptr[index];
  }

 private:
  const char *ptr;
  size_t size;
};

};  // namespace pdp

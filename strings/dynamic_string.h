#pragma once

#include "data/allocator.h"
#include "data/non_copyable.h"
#include "string_slice.h"

namespace pdp {

namespace impl {

struct _DynamicStringPrivInit;

}  // namespace impl

struct DynamicString : public NonCopyable {
  friend struct impl::_DynamicStringPrivInit;

  DynamicString() : ptr(nullptr), size(0) {}

  DynamicString(const char *str, size_t len) {
    ptr = Allocate<char>(allocator, len + 1);
    memcpy(ptr, str, len);
    ptr[len] = 0;
    size = len;
  }

  DynamicString(const char *begin, const char *end) : DynamicString(begin, end - begin) {}

  explicit DynamicString(const StringSlice &rhs) : DynamicString(rhs.Begin(), rhs.Size()) {}

  DynamicString(DynamicString &&rhs) : ptr(rhs.ptr), size(rhs.size) { rhs.ptr = nullptr; }

  void operator=(DynamicString &&rhs) = delete;

  ~DynamicString() {
    if (ptr) {
      Deallocate<char>(allocator, ptr);
    }
  }

  const char *Data() const { return ptr; }
  const char *Begin() const { return ptr; }
  const char *End() const { return ptr + size; }

  constexpr bool Empty() const { return size == 0; }
  constexpr size_t Size() const { return size; }
  constexpr size_t Length() const { return size; }

  const StringSlice GetSlice() const { return StringSlice(ptr, size); }

  bool operator==(const DynamicString &other) const { return GetSlice() == other.GetSlice(); }

  bool operator==(const StringSlice &other) const { return GetSlice() == other; }

  bool operator!=(const DynamicString &other) const { return !(*this == other); }

  bool operator!=(const StringSlice &other) const { return !(*this == other); }

  const char &operator[](size_t index) const { return ptr[index]; }

 private:
  char *ptr;
  size_t size;

  DefaultAllocator allocator;
};

template <>
struct IsReallocatable<DynamicString> : std::true_type {};

namespace impl {

struct _DynamicStringPrivInit {
  _DynamicStringPrivInit(DynamicString &s) : str(s) {}

  char *operator()(size_t length) {
    pdp_assert(!str.ptr);
    char *buf = Allocate<char>(str.allocator, length + 1);
#ifdef PDP_ENABLE_ZERO_INITIALIZE
    memset(buf, 0, length + 1);
#else
    buf[length] = 0;
#endif
    str.ptr = buf;
    str.size = length;
    return buf;
  }

  void operator()(char *data, size_t length) {
    pdp_assert(data[length] == '\0');
    pdp_assert(!str.ptr);
    str.ptr = data;
    str.size = length;
  }

 private:
  DynamicString &str;
};

}  // namespace impl

};  // namespace pdp

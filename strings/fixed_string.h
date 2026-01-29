#pragma once

#include "data/allocator.h"
#include "data/non_copyable.h"
#include "data/unique_ptr.h"
#include "string_slice.h"
#include "string_vector.h"

namespace pdp {

struct FixedString : public NonCopyable {
  FixedString() : ptr(nullptr), size(0) {}

  FixedString(const char *str, size_t len) {
    ptr = Allocate<char>(allocator, len + 1);
    memcpy(ptr, str, len);
    ptr[len] = 0;
    size = len;
  }

  FixedString(const char *begin, const char *end) : FixedString(begin, end - begin) {}

  explicit FixedString(const StringSlice &rhs) : FixedString(rhs.Begin(), rhs.Size()) {}

  FixedString(FixedString &&rhs) : ptr(rhs.ptr), size(rhs.size) { rhs.ptr = nullptr; }

  FixedString(StringVector &&rhs) {
    pdp_assert(rhs.Size() >= 1);
    pdp_assert(rhs.Last() == '\0');
    size = rhs.Size() - 1;
    impl::_VectorPrivAcess<char, DefaultAllocator> _vector_priv(rhs);
    ptr = _vector_priv.ReleaseData();
    pdp_assert(ptr[size] == 0);
  }

  FixedString(StringBuffer &&rhs, size_t length) {
    pdp_assert(rhs.Get() != nullptr && length > 0);
    pdp_assert(rhs[length] == '\0');
    ptr = rhs.Release();
    size = length;
    pdp_assert(ptr[size] == 0);
  }

  void operator=(FixedString &&rhs) = delete;

  ~FixedString() {
    if (ptr) {
      Deallocate<char>(allocator, ptr);
    }
  }

  FixedString Copy() const { return FixedString(Begin(), Size()); }

  void Reset(FixedString &&rhs) {
    pdp_assert(this != &rhs);
    if (PDP_LIKELY(ptr)) {
      Deallocate<char>(allocator, ptr);
    }
    ptr = rhs.ptr;
    size = rhs.size;
    rhs.ptr = nullptr;
  }

  void Reset(const StringSlice &rhs) {
    if (PDP_LIKELY(ptr)) {
      const bool insufficient_memory = (allocator.GetAllocationSize(ptr) < rhs.Size() + 1);
      if (insufficient_memory) {
        ptr = Reallocate<char>(allocator, ptr, rhs.Size() + 1);
      }
    }

    memcpy(ptr, rhs.Begin(), rhs.Size());
    size = rhs.Size();
    ptr[size] = 0;
  }

  const char *Cstr() const { return ptr; }
  const char *Begin() const { return ptr; }
  const char *End() const { return ptr + size; }

  constexpr bool Empty() const { return size == 0; }
  constexpr size_t Size() const { return size; }
  constexpr size_t Length() const { return size; }

  const StringSlice ToSlice() const { return StringSlice(ptr, size); }

  explicit operator StringSlice() const { return ToSlice(); }

  bool operator==(const FixedString &other) const { return ToSlice() == other.ToSlice(); }

  bool operator==(const StringSlice &other) const { return ToSlice() == other; }

  bool operator!=(const FixedString &other) const { return !(*this == other); }

  bool operator!=(const StringSlice &other) const { return !(*this == other); }

  const char &operator[](size_t index) const { return ptr[index]; }

 private:
  char *ptr;
  size_t size;

  DefaultAllocator allocator;
};

template <>
struct CanReallocate<FixedString> : std::true_type {};

};  // namespace pdp

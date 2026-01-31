#include "fixed_string.h"

namespace pdp {

FixedString::FixedString() : ptr(nullptr), size(0) {}

FixedString::FixedString(const char *str, size_t len) {
  ptr = Allocate<char>(allocator, len + 1);
  memcpy(ptr, str, len);
  ptr[len] = 0;
  size = len;
}

FixedString::FixedString(const char *begin, const char *end) : FixedString(begin, end - begin) {}

FixedString::FixedString(const StringSlice &rhs) : FixedString(rhs.Begin(), rhs.Size()) {}

FixedString::FixedString(FixedString &&rhs) : ptr(rhs.ptr), size(rhs.size) { rhs.ptr = nullptr; }

FixedString::FixedString(StringVector &&rhs) {
  pdp_assert(rhs.Size() >= 1);
  pdp_assert(rhs.Last() == '\0');
  size = rhs.Size() - 1;
  impl::_VectorPrivAcess<char, DefaultAllocator> _vector_priv(rhs);
  ptr = _vector_priv.ReleaseData();
  pdp_assert(ptr[size] == 0);
}

FixedString::FixedString(StringBuffer &&rhs, size_t length) {
  pdp_assert(rhs.Get() != nullptr);
  ptr = rhs.Release();
  size = length;
  pdp_assert(ptr[size] == 0);
}

FixedString::~FixedString() {
  if (ptr) {
    Deallocate<char>(allocator, ptr);
  }
}

FixedString FixedString::Copy() const { return FixedString(Begin(), Size()); }

void FixedString::Reset(FixedString &&rhs) {
  pdp_assert(this != &rhs);
  if (PDP_LIKELY(ptr)) {
    Deallocate<char>(allocator, ptr);
  }
  ptr = rhs.ptr;
  size = rhs.size;
  rhs.ptr = nullptr;
}

void FixedString::Reset(const StringSlice &rhs) {
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

const char *FixedString::Cstr() const { return ptr; }
const char *FixedString::Begin() const { return ptr; }
const char *FixedString::End() const { return ptr + size; }

bool FixedString::Empty() const { return size == 0; }
size_t FixedString::Size() const { return size; }
size_t FixedString::Length() const { return size; }

const StringSlice FixedString::ToSlice() const { return StringSlice(ptr, size); }

FixedString::operator StringSlice() const { return ToSlice(); }

bool FixedString::operator==(const FixedString &other) const {
  return ToSlice() == other.ToSlice();
}

bool FixedString::operator==(const StringSlice &other) const { return ToSlice() == other; }

bool FixedString::operator!=(const FixedString &other) const { return !(*this == other); }

bool FixedString::operator!=(const StringSlice &other) const { return !(*this == other); }

const char &FixedString::operator[](size_t index) const { return ptr[index]; }

};  // namespace pdp

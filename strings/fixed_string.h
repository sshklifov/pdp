#pragma once

#include "data/allocator.h"
#include "data/non_copyable.h"
#include "data/unique_ptr.h"
#include "string_slice.h"
#include "string_vector.h"

namespace pdp {

struct FixedString : public NonCopyable {
  FixedString();

  FixedString(const char *str, size_t len);
  FixedString(const char *begin, const char *end);

  explicit FixedString(const StringSlice &rhs);

  FixedString(FixedString &&rhs);
  explicit FixedString(StringVector &&rhs);
  FixedString(StringBuffer &&rhs, size_t length);

  void operator=(FixedString &&rhs) = delete;

  ~FixedString();

  FixedString Copy() const;
  void Reset(FixedString &&rhs);
  void Reset(const StringSlice &rhs);

  const char *Cstr() const;
  const char *Begin() const;
  const char *End() const;

  bool Empty() const;
  size_t Size() const;
  size_t Length() const;

  const StringSlice ToSlice() const;

  explicit operator StringSlice() const;

  bool operator==(const FixedString &other) const;
  bool operator==(const StringSlice &other) const;
  bool operator!=(const FixedString &other) const;
  bool operator!=(const StringSlice &other) const;

  const char &operator[](size_t index) const;

 private:
  char *ptr;
  size_t size;

  DefaultAllocator allocator;
};

template <>
struct CanReallocate<FixedString> : std::true_type {};

};  // namespace pdp

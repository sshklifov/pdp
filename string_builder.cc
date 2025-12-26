#include "string_builder.h"

namespace pdp {

size_t StringBuilder::Length() const { return size; }

StringSlice StringBuilder::Substr(size_t pos) const {
  pdp_assert(pos < Size());
  return StringSlice(ptr + pos, size - pos);
}

StringSlice StringBuilder::Substr(size_t pos, size_t n) const {
  pdp_assert(pos < Size() && pos + n <= size);
  return StringSlice(ptr + pos, n);
}

StringSlice StringBuilder::Substr(const char *it) const {
  pdp_assert(it >= ptr && it <= End());
  return StringSlice(it, End());
}

StringSlice StringBuilder::GetSlice() const { return StringSlice(ptr, size); }

bool StringBuilder::operator==(const StringSlice &other) const {
  if (Size() != other.Size()) {
    return false;
  }
  return memcmp(Begin(), other.Begin(), Size()) == 0;
}

bool StringBuilder::operator!=(const StringSlice &other) const { return !(*this == other); }

void StringBuilder::AppendfUnchecked(const StringSlice &fmt) {
#ifdef PDP_ENABLE_ASSERT
  StringSlice copy = fmt;
  auto it = copy.Find('{');
  while (it < copy.End()) {
    copy.DropLeft(it + 1);
    if (copy.StartsWith('}')) {
      OnAssertFailed("Extra {} in format string", fmt.Begin(), fmt.Size());
    }
    it = fmt.Find('{');
  }
#endif

  const bool more_work = !fmt.Empty();
  if (more_work) {
    Append(fmt);
  }
}

void StringBuilder::AppendUnchecked(char c) {
  pdp_assert(size + 1 <= capacity);
  ptr[size++] = c;
}

void StringBuilder::AppendUnchecked(const StringSlice &s) {
  pdp_assert(size + s.Size() <= capacity);
  memcpy(ptr + size, s.Begin(), s.Size());
  size += s.Size();
}

void StringBuilder::AppendUnchecked(void *p) {
  AppendUnchecked('0');
  AppendUnchecked('x');
  AppendUnchecked(reinterpret_cast<size_t>(p));
}

StringBuilder::StringBuilder() : LinearArray<char>(default_buffer_capacity) {}

};  // namespace pdp

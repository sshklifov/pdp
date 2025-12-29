#include "string_slice.h"
#include "core/check.h"

#include <cstring>

namespace pdp {

StringSlice::StringSlice(const char *p, size_t sz) : ptr(p), size(sz) {}

StringSlice::StringSlice(const char *begin, const char *end) : ptr(begin), size(end - begin) {
  pdp_assert(begin <= end);
}

StringSlice::StringSlice(const char *s) : ptr(s), size(strlen(s)) {}

const char *StringSlice::Data() const { return ptr; }
const char *StringSlice::Begin() const { return ptr; }
const char *StringSlice::End() const { return ptr + size; }

const char *StringSlice::Find(char c) const {
  const char *it = (const char *)memchr(ptr, c, size);
  return it != nullptr ? it : End();
}

const char *StringSlice::Find(const char *it, char c) const {
  pdp_assert(it >= Begin() && it <= End());
  const char *ret = (const char *)memchr(it, c, End() - it);
  return ret != nullptr ? ret : End();
}

StringSlice StringSlice::Substr(size_t pos) const {
  pdp_assert(pos < Size());
  return StringSlice(ptr + pos, size - pos);
}

StringSlice StringSlice::Substr(size_t pos, size_t n) const {
  pdp_assert(pos < Size() && pos + n <= size);
  return StringSlice(ptr + pos, size - pos);
}

StringSlice StringSlice::TakeLeft(const char *it) {
  pdp_assert(it >= ptr && it <= End());
  StringSlice res(ptr, it);
  ptr = it;
  return res;
}

void StringSlice::DropLeft(const char *it) {
  pdp_assert(it >= Begin() && it <= End());
  size -= it - ptr;
  ptr = it;
}

void StringSlice::DropLeft(size_t n) {
  pdp_assert(n <= Size());
  ptr += n;
  size -= n;
}

bool StringSlice::StartsWith(char c) const { return !Empty() && *ptr == c; }

bool StringSlice::Empty() const { return size == 0; }
size_t StringSlice::Size() const { return size; }
size_t StringSlice::Length() const { return size; }

bool StringSlice::operator==(const StringSlice &other) const {
  if (Size() != other.Size()) {
    return false;
  }
  return memcmp(Begin(), other.Begin(), Size()) == 0;
}

bool StringSlice::operator!=(const StringSlice &other) const { return !(*this == other); }

const char &StringSlice::operator[](size_t index) const { return ptr[index]; }

};  // namespace pdp

#include "string_view.h"
#include "check.h"

#include <cstring>

namespace pdp {

StringView::StringView(const char *p, size_t sz) : ptr(p), size(sz) {}

StringView::StringView(const char *begin, const char *end) : ptr(begin), size(end - begin) {
  pdp_silent_assert(begin <= end);
}

StringView::StringView(const char *s) : ptr(s), size(strlen(s)) {}

const char *StringView::Data() const { return ptr; }
const char *StringView::Begin() const { return ptr; }
const char *StringView::End() const { return ptr + size; }

const char *StringView::Find(char c) const {
  const char *it = (const char *)memchr(ptr, c, size);
  return it != nullptr ? it : End();
}

StringView StringView::Substr(size_t pos) const {
  pdp_silent_assert(pos < Size());
  return StringView(ptr + pos, size - pos);
}

StringView StringView::Substr(size_t pos, size_t n) const {
  pdp_silent_assert(pos < Size() && pos + n <= size);
  return StringView(ptr + pos, size - pos);
}

StringView StringView::TakeLeft(const char *it) {
  pdp_silent_assert(it >= ptr && it <= End());
  StringView res(ptr, it);
  ptr = it;
  return res;
}

void StringView::DropLeft(const char *it) {
  pdp_silent_assert(it >= Begin() && it <= End());
  size -= it - ptr;
  ptr = it;
}

void StringView::DropLeft(size_t n) {
  pdp_silent_assert(n <= Size());
  ptr += n;
  size -= n;
}

bool StringView::StartsWith(char c) const { return !Empty() && *ptr == c; }

bool StringView::Empty() const { return size == 0; }
size_t StringView::Size() const { return size; }
size_t StringView::Length() const { return size; }

bool StringView::operator==(const StringView &other) const {
  if (Size() != other.Size()) {
    return false;
  }
  return strncmp(Begin(), other.Begin(), Size()) == 0;
}

bool StringView::operator!=(const StringView &other) const { return !(*this == other); }

const char &StringView::operator[](size_t index) const { return ptr[index]; }

};  // namespace pdp

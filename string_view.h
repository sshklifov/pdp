#pragma once

#include <cstddef>

namespace pdp {

struct StringView {
  StringView(const char *p, size_t sz);
  StringView(const char *begin, const char *end);
  StringView(const char *s);

  const char *Data() const;
  const char *Begin() const;
  const char *End() const;

  const char *Find(char c) const;

  void DropPrefix(const char *it);
  void DropPrefix(size_t n);

  bool StartsWith(const StringView &other) const;
  bool StartsWith(char c) const;

  bool Empty() const;
  size_t Size() const;
  size_t Length() const;

  bool operator==(const StringView &other) const;
  bool operator!=(const StringView &other) const;

  const char &operator[](size_t index) const;

 private:
  const char *ptr;
  size_t size;
};

};  // namespace pdp

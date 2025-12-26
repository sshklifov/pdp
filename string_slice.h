#pragma once

#include <cstddef>

namespace pdp {

struct StringSlice {
  StringSlice(const char *p, size_t sz);
  StringSlice(const char *begin, const char *end);
  StringSlice(const char *s);

  const char *Data() const;
  const char *Begin() const;
  const char *End() const;

  const char *Find(char c) const;

  StringSlice Substr(size_t pos) const;
  StringSlice Substr(size_t pos, size_t n) const;

  StringSlice TakeLeft(const char *it);

  void DropLeft(const char *it);
  void DropLeft(size_t n);

  bool StartsWith(char c) const;

  bool Empty() const;
  size_t Size() const;
  size_t Length() const;

  bool operator==(const StringSlice &other) const;
  bool operator!=(const StringSlice &other) const;

  const char &operator[](size_t index) const;

 private:
  const char *ptr;
  size_t size;
};

};  // namespace pdp

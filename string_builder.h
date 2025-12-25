#pragma once

#include <cassert>
#include <cstdlib>
#include <cstring>

#include <exception>
#include <limits>
#include <tuple>

namespace pdp {

struct StringView {
  StringView(const char *p, size_t sz) : ptr(p), size(sz) {}

  StringView(const char *begin, const char *end) : ptr(begin), size(end - begin) {
    assert(begin <= end);
  }

  StringView(const char *s) : ptr(s), size(strlen(s)) {}

  const char *Begin() const { return ptr; }
  const char *End() const { return ptr + size; }

  const char *Find(char c) const {
    size_t pos = 0;
    while (pos < Size()) {
      if (ptr[pos] == c) {
        return ptr + pos;
      }
      ++pos;
    }
    return ptr + pos;
  }

  void DropPrefix(const char *it) {
    assert(it >= Begin() && it <= End());
    size -= it - ptr;
    ptr = it;
  }

  void DropPrefix(size_t n) {
    assert(n <= Size());
    ptr += n;
    size -= n;
  }

  bool StartsWith(const StringView &other) const {
    return Size() >= other.Size() && strncmp(Begin(), other.Begin(), other.Size()) == 0;
  }

  bool StartsWith(char c) const { return !Empty() && *ptr == c; }

  bool Empty() const { return size == 0; }
  size_t Size() const { return size; }
  size_t Length() const { return size; }

  bool operator==(const StringView &other) const {
    if (Size() != other.Size()) {
      return false;
    }
    return strncmp(Begin(), other.Begin(), Size()) == 0;
  }

  bool operator!=(const StringView &other) const { return !(*this == other); }

  const char &operator[](size_t index) const { return ptr[index]; }

 private:
  const char *ptr;
  size_t size;
};

inline size_t EstimateSize(const StringView &s) { return s.Size(); }

inline size_t EstimateSize(char c) { return 1; }

template <typename T>
inline std::enable_if_t<std::is_integral_v<T>, size_t> EstimateSize(T) {
  // Max decimal digits for 64-bit + sign
  return std::numeric_limits<T>::digits10 + 2;
}

struct Buffer {
  Buffer(Buffer &&other) : ptr(other.ptr), size(other.size), capacity(other.capacity) {
    other.ptr = nullptr;
  }

  Buffer(const Buffer &) = delete;

  Buffer &operator=(Buffer &&other) {
    if (this != &other) {
      ptr = other.ptr;
      size = other.size;
      capacity = other.capacity;
      other.ptr = nullptr;
    }
    return (*this);
  }

  void operator=(const Buffer &) = delete;

  ~Buffer() { free(ptr); }

  const char *Begin() const { return ptr; }
  const char *End() const { return ptr + size; }

  char *Begin() { return ptr; }
  char *End() { return ptr + size; }

  StringView Substr(size_t pos) const { return StringView(ptr + pos, size - pos); }

  StringView Substr(size_t pos, size_t n) const { return StringView(ptr + pos, n); }

  StringView Substr(const char *it) const { return StringView(it, End()); }

  StringView ViewOnly() const { return StringView(ptr, size); }

  bool Empty() const { return size == 0; }
  size_t Size() const { return size; }
  size_t Length() const { return size; }

  size_t Capacity() const { return capacity; }

  bool operator==(const StringView &other) const {
    if (Size() != other.Size()) {
      return false;
    }
    return strncmp(Begin(), other.Begin(), Size()) == 0;
  }

  bool operator!=(const StringView &other) const { return !(*this == other); }

  /// Clears this buffer.
  void Clear() { size = 0; }

  // Tries increasing the buffer capacity to `new_capacity`. It can increase the
  // capacity by a smaller amount than requested but guarantees there is space
  // for at least one additional element either by increasing the capacity or by
  // flushing the buffer if it is full.
  void Reserve(size_t new_capacity) {
    if (new_capacity > capacity) {
      GrowExtra(new_capacity - capacity);
    }
  }

  Buffer &operator+=(char c) {
    Append(c);
    return (*this);
  }

  /// Appends data to the end of the buffer.
  void Append(const char *begin, const char *end) { Append(StringView(begin, end)); }

  void Append(char c, size_t n) {
    Reserve(size + n);
    memset(ptr + size, c, n);
    size += n;
  }

  template <typename T>
  void Append(T value) {
    Reserve(size + EstimateSize(value));
    AppendUnchecked(value);
  }

  // Similar to printf but supports only %s and %d.
  template <typename... Args>
  void Appendf(const StringView &fmt, Args... args) {
    size_t req_size = fmt.Size();
    ((req_size += EstimateSize(args)), ...);
    Reserve(size + req_size);
    AppendfUnchecked(fmt, std::forward<Args>(args)...);
  }

  char &operator[](size_t index) {
    assert(index < Size());
    return ptr[index];
  }

  const char &operator[](size_t index) const {
    assert(index < Size());
    return ptr[index];
  }

 protected:
  Buffer(size_t cap) noexcept : ptr(static_cast<char *>(malloc(cap))), size(0), capacity(cap) {}

 private:
  template <typename Arg, typename... Args>
  void AppendfUnchecked(StringView fmt, Arg arg, Args... rest) {
    auto it = fmt.Find('{');
    AppendUnchecked(StringView(fmt.Begin(), it));
    fmt.DropPrefix(it);

    const bool more_fmt_data = !fmt.Empty();
    assert(more_fmt_data);
    if (__builtin_expect(more_fmt_data, true)) {
      fmt.DropPrefix(1);
      if (fmt.StartsWith('}')) {
        AppendUnchecked(arg);
        fmt.DropPrefix(1);
        return AppendfUnchecked(fmt, std::forward<Args>(rest)...);
      } else {
        AppendUnchecked('{');
        fmt.DropPrefix(1);
        return AppendfUnchecked(fmt, arg, std::forward<Args>(rest)...);
      }
    }
  }

  void AppendfUnchecked(const StringView &fmt) {
#ifndef NDEBUG
    // TODO
#endif
    const bool more_work = !fmt.Empty();
    if (more_work) {
      Append(fmt);
    }
  }

  void AppendUnchecked(char c) {
    assert(size + 1 <= capacity);
    ptr[size++] = c;
  }

  void AppendUnchecked(const StringView &s) {
    assert(size + s.Size() <= capacity);
    memcpy(ptr + size, s.Begin(), s.Size());
    size += s.Size();
  }

  template <typename T, std::enable_if_t<std::is_signed_v<T> && !std::is_same_v<T, char>, int> = 0>
  void AppendUnchecked(T signed_value) {
    using U = std::make_unsigned_t<T>;
    if (signed_value < 0) {
      AppendUnchecked('-');
      size_t magnitude = static_cast<U>(-(signed_value + 1)) + 1;
      AppendUnchecked(magnitude);
    } else {
      size_t magnitude = static_cast<U>(signed_value);
      AppendUnchecked(magnitude);
    }
  }

  template <typename U,
            std::enable_if_t<std::is_unsigned_v<U> && !std::is_same_v<U, char>, int> = 0>
  void AppendUnchecked(U unsigned_value) {
    char *write_head = ptr + size;
    do {
      assert(write_head < ptr + capacity);
      *write_head = '0' + (unsigned_value % 10);
      ++write_head;
      unsigned_value /= 10;
    } while (unsigned_value != 0);

    char *left = ptr + size;
    char *right = write_head - 1;
    size += write_head - left;

    while (left < right) {
      std::swap(*left, *right);
      ++left;
      --right;
    }
  }

  void GrowExtra(const size_t req_capacity) {
    size_t half_capacity = capacity / 2;
    size_t grow_capacity = half_capacity > req_capacity ? half_capacity : req_capacity;
    const size_t max_capacity = std::numeric_limits<size_t>::max();
    bool no_overflow = max_capacity - grow_capacity >= capacity;
    if (__builtin_expect(no_overflow, true)) {
      capacity += grow_capacity;
      ptr = static_cast<char *>(realloc(ptr, capacity));
      assert(ptr);
    } else {
      no_overflow = max_capacity - req_capacity >= capacity;
      if (no_overflow) {
        capacity += req_capacity;
        ptr = static_cast<char *>(realloc(ptr, capacity));
        assert(ptr);
      } else {
        std::terminate();
      }
    }
  }

  char *ptr;
  size_t size;
  size_t capacity;
};

struct StringBuilder : public Buffer {
  explicit StringBuilder() : Buffer(default_buffer_capacity) {}

 private:
  static constexpr const size_t default_buffer_capacity = 500;
};

};  // namespace pdp

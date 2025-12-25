#pragma once

#include "string_view.h"

#include <cassert>
#include <limits>
#include <type_traits>
#include <utility>

namespace pdp {

size_t EstimateSize(const StringView &s);

size_t EstimateSize(char c);

size_t EstimateSize(void *p);

template <typename T>
std::enable_if_t<std::is_integral_v<T>, size_t> EstimateSize(T) {
  // Max decimal digits for 64-bit + sign
  return std::numeric_limits<T>::digits10 + 2;
}

struct Buffer {
  Buffer(size_t cap) noexcept;
  Buffer(Buffer &&other);

  Buffer(const Buffer &) = delete;

  Buffer &operator=(Buffer &&other);

  void operator=(const Buffer &) = delete;

  ~Buffer();

  const char *Data() const;
  char *Data();

  const char *Begin() const;
  const char *End() const;

  char *Begin();
  char *End();

  StringView Substr(size_t pos) const;
  StringView Substr(size_t pos, size_t n) const;
  StringView Substr(const char *it) const;

  StringView ViewOnly() const;

  bool Empty() const;
  size_t Size() const;
  size_t Length() const;
  size_t Capacity() const;

  bool operator==(const StringView &other) const;
  bool operator!=(const StringView &other) const;

  /// Clears this buffer.
  void Clear();

  // Tries increasing the buffer capacity to `new_capacity`. It can increase the
  // capacity by a smaller amount than requested but guarantees there is space
  // for at least one additional element either by increasing the capacity or by
  // flushing the buffer if it is full.
  void Reserve(size_t new_capacity);
  void Grow();

  Buffer &operator+=(char c);

  /// Appends data to the end of the buffer.
  void Append(const char *begin, const char *end);
  void Append(char c, size_t n);

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

  char &operator[](size_t index);

  const char &operator[](size_t index) const;

 private:
  template <typename Arg, typename... Args>
  void AppendfUnchecked(StringView fmt, Arg arg, Args... rest) {
    auto it = fmt.Find('{');
    AppendUnchecked(StringView(fmt.Begin(), it));
    fmt.DropPrefix(it);

    const bool more_fmt_data = !fmt.Empty();
    assert(more_fmt_data);
    // XXX: This is very intentionally not using PDP_LIKELY.
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

  void AppendfUnchecked(const StringView &fmt);
  void AppendUnchecked(char c);
  void AppendUnchecked(const StringView &s);
  void AppendUnchecked(void *p);

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

  void GrowExtra(const size_t req_capacity);

  char *ptr;
  size_t size;
  size_t capacity;
};

struct StringBuilder : public Buffer {
  explicit StringBuilder();

 private:
  // Stays within glibc tcache small-bin range. Does that matter? Probably less than I think.
  static constexpr const size_t default_buffer_capacity = 500;
};

};  // namespace pdp

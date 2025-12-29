#pragma once

#include "data/vector.h"
#include "string_slice.h"

namespace pdp {

struct EstimateSize {
  constexpr size_t operator()(const StringSlice &s) { return s.Size(); }

  constexpr size_t operator()(char c) { return 1; }

  constexpr size_t operator()(void *p) {
    // Max decimal digits for 64-bit + "0x" prefix
    return std::numeric_limits<size_t>::digits10 + 3;
  }

  template <typename T>
  constexpr std::enable_if_t<std::is_integral_v<T>, size_t> operator()(T) {
    // Max decimal digits + sign
    return std::numeric_limits<T>::digits10 + 2;
  }
};

template <typename Alloc = DefaultAllocator>
struct StringBuilder : public Vector<char, Alloc> {
  StringBuilder() = default;

  StringBuilder(size_t cap) : Vector<char, Alloc>(cap) {}

  size_t Length() const { return this->Size(); }

  StringSlice Substr(size_t pos) const {
    pdp_assert(pos < this->Size());
    return StringSlice(this->ptr + pos, this->Size() - pos);
  }

  StringSlice Substr(size_t pos, size_t n) const {
    pdp_assert(pos < this->Size() && pos + n <= this->size);
    return StringSlice(this->ptr + pos, n);
  }

  StringSlice Substr(const char *it) const {
    pdp_assert(it >= this->ptr && it <= this->End());
    return StringSlice(it, this->End());
  }

  StringSlice GetSlice() const { return StringSlice(this->ptr, this->Size()); }

  bool operator==(const StringSlice &other) const {
    if (this->Size() != other.Size()) {
      return false;
    }
    return memcmp(this->Begin(), other.Begin(), this->Size()) == 0;
  }

  bool operator!=(const StringSlice &other) const { return !(*this == other); }

  template <typename T>
  void Append(T value) {
    EstimateSize estimator;
    this->ReserveFor(estimator(value));
    AppendUnchecked(value);
  }

  // Similar to printf but supports only %s and %d.
  template <typename... Args>
  void Appendf(const StringSlice &fmt, Args... args) {
    size_t req_size = fmt.Size();
    EstimateSize estimator;
    ((req_size += estimator(args)), ...);
    this->ReserveFor(req_size);
    AppendfUnchecked(fmt, std::forward<Args>(args)...);
  }

  template <typename Arg, typename... Args>
  void AppendfUnchecked(StringSlice fmt, Arg arg, Args... rest) {
#ifdef PDP_ENABLE_ASSERT
    StringSlice original_fmt = fmt;
#endif

    auto it = fmt.Find('{');
    AppendUnchecked(StringSlice(fmt.Begin(), it));
    fmt.DropLeft(it);

    const bool more_fmt_data = !fmt.Empty();
    // XXX: This is very intentionally not using PDP_LIKELY.
    if (__builtin_expect(more_fmt_data, true)) {
      fmt.DropLeft(1);
      if (fmt.StartsWith('}')) {
        AppendUnchecked(arg);
        fmt.DropLeft(1);
        return AppendfUnchecked(fmt, std::forward<Args>(rest)...);
      } else {
        AppendUnchecked('{');
        return AppendfUnchecked(fmt, arg, std::forward<Args>(rest)...);
      }
    } else {
#ifdef PDP_ENABLE_ASSERT
      OnAssertFailed("Extra arguments for format", original_fmt.Begin(), original_fmt.Size());
#endif
    }
  }

  void AppendfUnchecked(const StringSlice &fmt) {
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

  void AppendUnchecked(char c) {
    pdp_assert(this->size + 1 <= this->capacity);
    this->ptr[this->size++] = c;
  }

  void AppendUnchecked(const StringSlice &s) {
    pdp_assert(this->size + s.Size() <= this->capacity);
    memcpy(this->End(), s.Begin(), s.Size());
    this->size += s.Size();
  }

  void AppendUnchecked(void *p) {
    AppendUnchecked('0');
    AppendUnchecked('x');
    AppendUnchecked(reinterpret_cast<size_t>(p));
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
    char *write_head = this->End();
    do {
      pdp_assert(write_head < this->ptr + this->capacity);
      *write_head = '0' + (unsigned_value % 10);
      ++write_head;
      unsigned_value /= 10;
    } while (unsigned_value != 0);

    char *left = this->End();
    char *right = write_head - 1;
    this->size += write_head - left;

    while (left < right) {
      std::swap(*left, *right);
      ++left;
      --right;
    }
  }
};

};  // namespace pdp

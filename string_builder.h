#pragma once

#include "linear_array.h"
#include "string_slice.h"

namespace pdp {

struct EstimateSize {
  size_t operator()(const StringSlice &s) { return s.Size(); }

  size_t operator()(char c) { return 1; }

  size_t operator()(void *p) {
    // Max decimal digits for 64-bit + "0x" prefix
    return std::numeric_limits<size_t>::digits10 + 3;
  }

  template <typename T>
  std::enable_if_t<std::is_integral_v<T>, size_t> operator()(T) {
    // Max decimal digits + sign
    return std::numeric_limits<T>::digits10 + 2;
  }
};

struct StringBuilder : public LinearArray<char> {
  explicit StringBuilder();

  size_t Length() const;

  StringSlice Substr(size_t pos) const;
  StringSlice Substr(size_t pos, size_t n) const;
  StringSlice Substr(const char *it) const;

  // TODO rename
  StringSlice ViewOnly() const;

  bool operator==(const StringSlice &other) const;
  bool operator!=(const StringSlice &other) const;

  template <typename T>
  void Append(T value) {
    EstimateSize estimator;
    ReserveFor(estimator(value));
    AppendUnchecked(value);
  }

  // Similar to printf but supports only %s and %d.
  template <typename... Args>
  void Appendf(const StringSlice &fmt, Args... args) {
    size_t req_size = fmt.Size();
    EstimateSize estimator;
    ((req_size += estimator(args)), ...);
    ReserveFor(req_size);
    AppendfUnchecked(fmt, std::forward<Args>(args)...);
  }

 private:
  // Stays within glibc tcache small-bin range. Does that matter? Probably less than I think.
  static constexpr const size_t default_buffer_capacity = 500;

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
        fmt.DropLeft(1);
        return AppendfUnchecked(fmt, arg, std::forward<Args>(rest)...);
      }
    } else {
#ifdef PDP_ENABLE_ASSERT
      OnSilentAssertFailed("Extra arguments for format", original_fmt.Begin(), original_fmt.Size());
#endif
    }
  }

  void AppendfUnchecked(const StringSlice &fmt);

  void AppendUnchecked(char c);
  void AppendUnchecked(const StringSlice &s);
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
      pdp_silent_assert(write_head < ptr + capacity);
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
};

};  // namespace pdp

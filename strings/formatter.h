#pragma once

#include "string_slice.h"

#include "core/internals.h"
#include "data/non_copyable.h"

#include <cstdint>
#include <limits>
#include <type_traits>
#include <utility>

namespace pdp {

template <typename T>
constexpr bool IsCStringV =
    std::is_pointer_v<std::decay_t<T>> &&
    std::is_same_v<std::remove_cv_t<std::remove_pointer_t<std::decay_t<T>>>, char>;

// Special formatters

struct Hex64 {
  uint64_t value;
};

template <typename T, std::enable_if_t<std::is_unsigned_v<T>, int> = 0>
Hex64 MakeHex(T value) {
  return Hex64{value};
}

struct ByteSize {
  uint64_t value;
};

template <typename T, std::enable_if_t<std::is_unsigned_v<T>, int> = 0>
ByteSize MakeByteSize(T value) {
  return ByteSize{value};
}

// Compile time size estimator (inexact).

template <typename T>
struct EstimateSize;

template <>
struct EstimateSize<char> : public std::integral_constant<unsigned, 1> {};

template <>
struct EstimateSize<bool> : public std::integral_constant<unsigned, 5> {};

template <typename T>
struct EstimateSize<T *> : public std::integral_constant<unsigned, sizeof(void *) * 2 + 2> {};

template <>
struct EstimateSize<uint32_t>
    : public std::integral_constant<unsigned, std::numeric_limits<uint32_t>::digits10 + 1> {};

template <>
struct EstimateSize<int32_t>
    : public std::integral_constant<unsigned, std::numeric_limits<int32_t>::digits10 + 2> {};

template <>
struct EstimateSize<uint64_t>
    : public std::integral_constant<unsigned, std::numeric_limits<uint64_t>::digits10 + 1> {};

template <>
struct EstimateSize<int64_t>
    : public std::integral_constant<unsigned, std::numeric_limits<int64_t>::digits10 + 2> {};

template <>
struct EstimateSize<ByteSize>
    : public std::integral_constant<unsigned, std::numeric_limits<uint32_t>::digits10> {};

template <typename T>
inline constexpr unsigned EstimateSizeV = EstimateSize<T>::value;

// Run time size estimator (exact).

inline uint32_t CountDigits10(uint64_t n) {
  static constexpr uint8_t table[] = {
      1,  1,  1,  2,  2,  2,  3,  3,  3,  4,  4,  4,  4,  5,  5,  5,  6,  6,  6,  7,  7,  7,
      7,  8,  8,  8,  9,  9,  9,  10, 10, 10, 10, 11, 11, 11, 12, 12, 12, 13, 13, 13, 13, 14,
      14, 14, 15, 15, 15, 16, 16, 16, 16, 17, 17, 17, 18, 18, 18, 19, 19, 19, 19, 20};
  // Compute bits-1 of n and use that to index into the table.
  uint8_t inc = table[PDP_CLZLL(n | 1) ^ 63];

#define POWERS_OF_10(factor)                                                         \
  factor * 10, (factor) * 100, (factor) * 1000, (factor) * 10000, (factor) * 100000, \
      (factor) * 1000000, (factor) * 10000000, (factor) * 100000000, (factor) * 1000000000
  static constexpr uint64_t powers_of_10[] = {0, 0, POWERS_OF_10(1U), POWERS_OF_10(1000000000ULL),
                                              10000000000000000000ULL};
#undef POWERS_OF_10

  // Fixes the log2 -> log10 off-by-one
  return inc - (n < powers_of_10[inc]);
}

inline uint32_t CountDigits16(uint64_t n) {
  uint32_t bits = PDP_CLZLL(n | 1) ^ 63;
  return (bits >> 2) + 1;
}

template <typename U, std::enable_if_t<std::is_unsigned_v<U>, int> = 0>
bool IsEqualDigits10(U unsigned_value, StringSlice str) {
  const uint64_t digits = CountDigits10(unsigned_value);
  if (PDP_LIKELY(str.Size() != digits)) {
    return false;
  }
  pdp_assert(!str.Empty());

  const char *__restrict__ it = str.End() - 1;
  do {
    char digit = '0' + (unsigned_value % 10);
    if (PDP_LIKELY(digit != *it)) {
      return false;
    }
    --it;
    unsigned_value /= 10;
  } while (unsigned_value > 0);

  return true;
}

template <typename U, typename T>
U BitCast(T value) {
  static_assert(sizeof(T) == sizeof(U));
  static_assert(std::is_trivially_copyable_v<T>);
  static_assert(std::is_trivially_copyable_v<U>);

  U result;
  memcpy(&result, &value, sizeof(T));
  return result;
}

template <typename T, std::enable_if_t<std::is_signed_v<T>, int> = 0>
std::make_unsigned_t<T> NegativeToUnsigned(T negative_value) {
  using U = std::make_unsigned_t<T>;
  U magnitude = BitCast<U>(negative_value);
  return ~magnitude + 1;
}

template <typename T, std::enable_if_t<std::is_signed_v<T>, int> = 0>
bool IsEqualDigits10(T signed_value, StringSlice str) {
  using U = std::make_unsigned_t<T>;
  if (signed_value < 0) {
    if (PDP_LIKELY(!str.StartsWith('-'))) {
      return false;
    }
    U magnitude = NegativeToUnsigned(signed_value);
    str.DropLeft(1);
    return IsEqualDigits10(magnitude, str);
  } else {
    U magnitude = BitCast<U>(signed_value);
    return IsEqualDigits10(magnitude, str);
  }
}

// Type erase classes

union PackedValue {
  PackedValue() = default;
  constexpr PackedValue(bool x) : _bool(x) {}
  constexpr PackedValue(char x) : _char(x) {}
  constexpr PackedValue(int32_t x) : _int32(x) {}
  constexpr PackedValue(uint32_t x) : _uint32(x) {}
  constexpr PackedValue(int64_t x) : _int64(x) {}
  constexpr PackedValue(uint64_t x) : _uint64(x) {}
  constexpr PackedValue(void *x) : _ptr(x) {}

  constexpr PackedValue(Hex64 x) : _hex64(x) {}
  constexpr PackedValue(ByteSize x) : _byte_size(x) {}

  bool _bool;
  char _char;
  int32_t _int32;
  uint32_t _uint32;
  int64_t _int64;
  uint64_t _uint64;
  const void *_ptr;
  const char *_str;
  Hex64 _hex64;
  ByteSize _byte_size;
};

enum PackedValueType {
  kUnspecified,
  kBool,
  kChar,
  kInt32,
  kUint32,
  kInt64,
  kUint64,
  kPtr,
  kStr,
  kByteSize,
};

template <typename T>
struct PackOneType;

template <>
struct PackOneType<bool> : std::integral_constant<uint64_t, kBool> {};

template <>
struct PackOneType<char> : std::integral_constant<uint64_t, kChar> {};

template <>
struct PackOneType<int32_t> : std::integral_constant<uint64_t, kInt32> {};

template <>
struct PackOneType<uint32_t> : std::integral_constant<uint64_t, kUint32> {};

template <>
struct PackOneType<int64_t> : std::integral_constant<uint64_t, kInt64> {};

template <>
struct PackOneType<uint64_t> : std::integral_constant<uint64_t, kUint64> {};

template <>
struct PackOneType<StringSlice> : std::integral_constant<uint64_t, kStr> {};

template <>
struct PackOneType<Hex64> : std::integral_constant<uint64_t, kPtr> {};

template <>
struct PackOneType<ByteSize> : std::integral_constant<uint64_t, kByteSize> {};

template <typename T>
struct PackOneType<T *> : std::integral_constant<uint64_t, kPtr> {};

template <typename T, typename = void>
struct IsPackable : std::false_type {};

template <typename T>
struct IsPackable<T, std::void_t<decltype(PackOneType<T>::value)>> : std::true_type {};

template <typename... Args>
constexpr uint64_t PackTypeBits() {
  uint64_t bits = 0;
  uint64_t shift = 0;

  ((bits |= (PackOneType<Args>::value << shift), shift += 4), ...);
  return bits;
}

template <typename T>
constexpr uint64_t PackOneSlots() {
  if constexpr (sizeof(T) <= sizeof(uint64_t)) {
    return 1;
  } else if constexpr (std::is_same_v<T, StringSlice>) {
    return 2;
  } else {
    static_assert(false, "Unsupported type!");
  }
}

template <typename... T>
constexpr size_t PackSlots() {
  return (0 + ... + PackOneSlots<T>());
}

template <typename... Args>
struct PackedArgs {
  static_assert(sizeof...(Args) <= 16, "Too many arguments passed!");

  constexpr PackedArgs(Args... args) : type_bits(PackTypeBits<Args...>()) {
    [[maybe_unused]]
    size_t i = 0;
    ((i += ConstructLoop(i, args)), ...);
  }

  static constexpr size_t kNumSlots = PackSlots<Args...>();

  uint64_t type_bits;
  PackedValue slots[kNumSlots];

 private:
  template <typename T>
  constexpr uint64_t ConstructLoop(size_t i, T value) {
    static_assert(!IsCStringV<T>, "Convert to StringSlice!");

    if constexpr (sizeof(T) <= sizeof(uint64_t)) {
      static_assert(PackOneSlots<T>() == 1);
      slots[i] = PackedValue(value);
      return 1;
    } else if constexpr (std::is_same_v<T, StringSlice>) {
      static_assert(PackOneSlots<T>() == 2);
      slots[i]._str = value.Begin();
      slots[i + 1]._uint64 = value.Size();
      return 2;
    } else {
      static_assert(false, "Unsupported type!");
    }
  }
};

template <typename... Args>
constexpr auto MakePackedArgs(Args &&...args) {
  using ResultType = PackedArgs<std::decay_t<Args>...>;
  return ResultType(std::forward<Args>(args)...);
}

template <typename T>
using PackableOrChar = std::conditional_t<IsPackable<T>::value, T, char>;

template <typename T>
constexpr auto PackUnknownArg(T &&a) {
  if constexpr (IsCStringV<T>) {
    return StringSlice(std::forward<T>(a));
  } else if constexpr (IsPackable<std::decay_t<T>>::value) {
    return std::forward<T>(a);
  } else {
    return '?';
  }
}

template <typename... Args>
constexpr auto MakePackedUnknownArgs(Args &&...args) {
  using ResultType = PackedArgs<decltype(PackUnknownArg(std::declval<Args>()))...>;
  return ResultType(PackUnknownArg(args)...);
}

inline size_t RunEstimator(PackedValue *args, uint64_t type_bits) {
  size_t bytes = 0;
  size_t i = 0;
  while (type_bits > 0) {
    switch (type_bits & 0xF) {
      case kBool:
        bytes += EstimateSizeV<decltype(args[i]._bool)>;
        break;
      case kChar:
        bytes += EstimateSizeV<decltype(args[i]._char)>;
        break;
      case kInt32:
        bytes += EstimateSizeV<decltype(args[i]._int32)>;
        break;
      case kUint32:
        bytes += EstimateSizeV<decltype(args[i]._uint32)>;
        break;
      case kInt64:
        bytes += EstimateSizeV<decltype(args[i]._int64)>;
        break;
      case kUint64:
        bytes += EstimateSizeV<decltype(args[i]._uint64)>;
        break;
      case kPtr:
        bytes += EstimateSizeV<decltype(args[i]._ptr)>;
        break;
      case kStr:
        ++i;
        bytes += args[i]._uint64;
        break;
      case kByteSize:
        bytes += EstimateSizeV<decltype(args[i]._byte_size)>;
        break;
      default:
        pdp_assert(false);
    }
    type_bits >>= 4;
    ++i;
  }
  return bytes;
}

struct Formatter : public NonCopyableNonMovable {
  Formatter(char *write_ptr, const char *limit_ptr) : end(write_ptr), limit(limit_ptr) {}

  template <size_t N>
  Formatter(char (&buf)[N]) : Formatter(buf, buf + N) {}

  char *End() { return end; }

  void AppendUnchecked(bool b) {
    if (b) {
      AppendUnchecked(StringSlice("true"));
    } else {
      AppendUnchecked(StringSlice("false"));
    }
  }

  void AppendUnchecked(char c) {
    pdp_assert(end < limit);
    *end = c;
    ++end;
  }

  void AppendUnchecked(const char *s) { AppendUnchecked(StringSlice(s)); }

  void AppendUnchecked(const StringSlice &s) {
    pdp_assert(limit - end >= (int64_t)s.Size());
    memcpy(end, s.Begin(), s.Size());
    end += s.Size();
  }

  template <typename U, std::enable_if_t<std::is_unsigned_v<U>, int> = 0>
  void AppendUnchecked(U unsigned_value) {
    const uint64_t digits = CountDigits10(unsigned_value);
    pdp_assert(limit - end >= (int64_t)digits);

    char *__restrict__ store = end + digits - 1;
    do {
      pdp_assert(store >= end);
      *store = '0' + (unsigned_value % 10);
      --store;
      unsigned_value /= 10;
    } while (unsigned_value != 0);

    pdp_assert(store == end - 1);
    end += digits;
  }

  void AppendUnchecked(const void *ptr) {
    size_t unsigned_value = reinterpret_cast<uint64_t>(ptr);
    const uint64_t digits = CountDigits16(unsigned_value);
    pdp_assert(limit - end >= (int64_t)digits + 2);

    const char *lookup = "0123456789abcdef";

    char *__restrict__ store = end + digits + 1;
    do {
      pdp_assert(store >= end);
      *store = lookup[unsigned_value & 0xf];
      --store;
      unsigned_value >>= 4;
    } while (unsigned_value != 0);

    *store = 'x';
    --store;
    *store = '0';
    pdp_assert(store == end);
    end = end + digits + 2;
  }

  template <typename T, std::enable_if_t<std::is_signed_v<T>, int> = 0>
  void AppendUnchecked(T signed_value) {
    using U = std::make_unsigned_t<T>;
    if (signed_value < 0) {
      pdp_assert(end < limit);
      *end = '-';
      ++end;
      U magnitude = NegativeToUnsigned(signed_value);
      AppendUnchecked(magnitude);
    } else {
      U magnitude = BitCast<U>(signed_value);
      AppendUnchecked(magnitude);
    }
  }

  void AppendUnchecked(Hex64 hex) { AppendUnchecked(BitCast<void *>(hex.value)); }

  void AppendUnchecked(ByteSize bytes) {
    auto digits = CountDigits16(bytes.value);
    byte table[17] = {1, 1, 1, 1, 2, 2, 3, 3, 3, 4, 4, 4, 4, 4, 4, 4, 4};
    byte unit = table[digits];
    uint64_t power = 1ull << ((unit << 3) | (unit << 1));

    // Correction
    unit -= (bytes.value < power);

    auto div = bytes.value >> ((unit << 3) | (unit << 1));
    AppendUnchecked(div);
    const char *lookup = "BKMGT";
    AppendUnchecked(lookup[unit]);
  }

  void AppendPackUnchecked(StringSlice fmt, PackedValue *args, uint64_t type_bits) {
#ifdef PDP_ENABLE_ASSERT
    StringSlice original_fmt = fmt;
#endif
    while (type_bits > 0) {
      const char *it = fmt.MemChar('{');
      if (PDP_UNLIKELY(!it)) {
#ifdef PDP_ENABLE_ASSERT
        OnFatalError("Extra arguments for format", original_fmt.Data(), original_fmt.Size());
#endif
        return;
      }
      StringSlice unformatted(fmt.Begin(), it);
      AppendUnchecked(unformatted);
      fmt.DropLeft(unformatted.Size() + 1);

      if (PDP_LIKELY(fmt.StartsWith('}'))) {
        const size_t num_slots_used = AppendPackedValueUnchecked(args, type_bits);
        args += num_slots_used;
        type_bits >>= 4;
        fmt.DropLeft(1);
      } else {
        AppendUnchecked('{');
      }
    }
#ifdef PDP_ENABLE_ASSERT
    if (memmem(fmt.Begin(), fmt.Size(), "{}", 2)) {
      OnFatalError("Insufficient arguments for format", original_fmt.Data(), original_fmt.Size());
    }
#endif
    AppendUnchecked(fmt);
  }

  size_t AppendPackedValueUnchecked(PackedValue *arg, uint64_t type_bits) {
    switch (type_bits & 0xF) {
      case kChar:
        AppendUnchecked(arg->_char);
        return PackOneSlots<decltype(arg->_char)>();
      case kBool:
        AppendUnchecked(arg->_bool);
        return PackOneSlots<decltype(arg->_bool)>();
      case kInt32:
        AppendUnchecked(arg->_int32);
        return PackOneSlots<decltype(arg->_int32)>();
      case kUint32:
        AppendUnchecked(arg->_uint32);
        return PackOneSlots<decltype(arg->_uint32)>();
      case kInt64:
        AppendUnchecked(arg->_int64);
        return PackOneSlots<decltype(arg->_int64)>();
      case kUint64:
        AppendUnchecked(arg->_uint64);
        return PackOneSlots<decltype(arg->_uint64)>();
      case kPtr:
        AppendUnchecked(arg->_ptr);
        return PackOneSlots<decltype(arg->_ptr)>();
      case kStr:
        AppendUnchecked(StringSlice(arg[0]._str, arg[1]._uint64));
        return PackOneSlots<StringSlice>();
      case kByteSize:
        AppendUnchecked(arg->_byte_size);
        return PackOneSlots<decltype(arg->_byte_size)>();
      default:
        pdp_assert(false);
        return 0;
    }
  }

 private:
  char *__restrict__ end;
  [[maybe_unused]] const char *__restrict__ limit;
};

}  // namespace pdp

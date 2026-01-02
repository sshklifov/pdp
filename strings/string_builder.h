#pragma once

#include "core/internals.h"
#include "data/allocator.h"
#include "string_slice.h"

#include <cstdint>
#include <limits>

namespace pdp {

template <typename T>
constexpr bool IsCStringV =
    std::is_pointer_v<std::decay_t<T>> &&
    std::is_same_v<std::remove_cv_t<std::remove_pointer_t<std::decay_t<T>>>, char>;

// Compile time size estimator (inexact).

struct EstimateSize {
  constexpr size_t operator()(const StringSlice &s) const { return s.Size(); }

  constexpr size_t operator()(bool b) const { return 5; }

  constexpr size_t operator()(char c) const { return 1; }

  template <typename T, std::enable_if_t<!IsCStringV<T>, int> = 0>
  constexpr size_t operator()(const T *p) const {
    // Max hex digits for 64-bit + "0x" prefix
    return sizeof(void *) * 2 + 2;
  }

  template <typename T>
  constexpr std::enable_if_t<std::is_unsigned_v<T>, size_t> operator()(T) const {
    return std::numeric_limits<T>::digits10 + 1;
  }

  template <typename T>
  constexpr std::enable_if_t<std::is_signed_v<T>, size_t> operator()(T) const {
    // Max decimal digits + sign
    return std::numeric_limits<T>::digits10 + 2;
  }
};

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
  static constexpr const uint64_t powers_of_10[] = {
      0, 0, POWERS_OF_10(1U), POWERS_OF_10(1000000000ULL), 10000000000000000000ULL};
#undef POWERS_OF_10

  // Fixes the log2 -> log10 off-by-one
  return inc - (n < powers_of_10[inc]);
}

inline uint32_t CountDigits16(uint64_t n) {
  uint32_t bits = PDP_CLZLL(n | 1) ^ 63;
  return (bits >> 2) + 1;
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

  bool _bool;
  char _char;
  int32_t _int32;
  uint32_t _uint32;
  int64_t _int64;
  uint64_t _uint64;
  const void *_ptr;
  const char *_str;
};

enum PackedValueType { kUnspecified, kBool, kChar, kInt32, kUint32, kInt64, kUint64, kPtr, kStr };

template <typename T>
constexpr uint64_t PackOneType() {
  if constexpr (std::is_same_v<T, bool>) {
    return kBool;
  } else if constexpr (std::is_same_v<T, char>) {
    return kChar;
  } else if constexpr (std::is_same_v<T, int32_t>) {
    return kInt32;
  } else if constexpr (std::is_same_v<T, uint32_t>) {
    return kUint32;
  } else if constexpr (std::is_same_v<T, int64_t>) {
    return kInt64;
  } else if constexpr (std::is_same_v<T, uint64_t>) {
    return kUint64;
  } else if constexpr (std::is_same_v<T, StringSlice>) {
    return kStr;
  } else if constexpr (std::is_pointer_v<T>) {
    return kPtr;
  } else {
    static_assert(false, "Unsupported type!");
  }
}

template <typename... Args>
constexpr uint64_t PackTypeBits() {
  uint64_t bits = 0;
  uint64_t shift = 0;

  ((bits |= (PackOneType<Args>() << shift), shift += 4), ...);
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

  static constexpr size_t Slots = PackSlots<Args...>();

  uint64_t type_bits;
  PackedValue slots[Slots];

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

inline size_t RunEstimator(PackedValue *args, uint64_t type_bits) {
  size_t bytes = 0;
  size_t i = 0;
  while (type_bits > 0) {
    EstimateSize estimator;
    switch (type_bits & 0xF) {
      case kBool:
        bytes += estimator(args[i]._bool);
        break;
      case kChar:
        bytes += estimator(args[i]._char);
        break;
      case kInt32:
        bytes += estimator(args[i]._int32);
        break;
      case kUint32:
        bytes += estimator(args[i]._uint32);
        break;
      case kInt64:
        bytes += estimator(args[i]._int64);
        break;
      case kUint64:
        bytes += estimator(args[i]._uint64);
        break;
      case kPtr:
        bytes += estimator(args[i]._ptr);
        break;
      case kStr:
        ++i;
        bytes += args[i]._uint64;
        break;
      default:
        pdp_assert(false);
    }
    type_bits >>= 4;
    ++i;
  }
  return bytes;
}

// TODO comment

template <typename Alloc = DefaultAllocator>
struct StringBuilder {
  StringBuilder() : begin(buffer), end(buffer), limit(buffer + sizeof(buffer)) {
#ifdef PDP_ENABLE_ZERO_INITIALIZE
    memset(buffer, 0, sizeof(buffer));
#endif
  }

  ~StringBuilder() {
    if (PDP_UNLIKELY(begin != buffer)) {
      Deallocate<char>(allocator, begin);
    }
  }

  char *Begin() { return begin; }
  const char *Begin() const { return begin; }

  const char *Data() const { return begin; }

  char *End() { return end; }
  const char *End() const { return end; }

  bool Empty() const { return begin == end; }
  size_t Length() const { return end - begin; }
  size_t Size() const { return Length(); }
  size_t Capacity() const { return limit - begin; }

  void Clear() { end = begin; }

  StringSlice GetSlice() const { return StringSlice(begin, end); }

  // Append methods.

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

  template <typename T, std::enable_if_t<!IsCStringV<T *>, int> = 0>
  void AppendUnchecked(const T *ptr) {
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
      size_t magnitude = ~static_cast<U>(signed_value) + 1;
      AppendUnchecked(magnitude);
    } else {
      size_t magnitude = static_cast<U>(signed_value);
      AppendUnchecked(magnitude);
    }
  }

  // Safe Append version.

  template <typename T, std::enable_if_t<std::is_integral_v<T>, int> = 0>
  void Append(T integral) {
    EstimateSize estimator;
    ReserveFor(estimator(integral));
    AppendUnchecked(integral);
  }

  template <typename T, std::enable_if_t<!IsCStringV<T *>, int> = 0>
  void Append(const T *ptr) {
    EstimateSize estimator;
    ReserveFor(estimator(ptr));
    AppendUnchecked((const void *)ptr);
  }

  void Append(const StringSlice &str) {
    ReserveFor(str.Size());
    AppendUnchecked(str);
  }

  // Append variadic arguments.

  template <typename... Args>
  void AppendFormat(const StringSlice &fmt, Args &&...args) {
    auto packed_args = MakePackedArgs(std::forward<Args>(args)...);
    AppendPack(fmt, packed_args.slots, packed_args.type_bits);
  }

  void AppendPack(const StringSlice &fmt, PackedValue *slots, uint64_t type_bits) {
    ReserveFor(fmt.Size() + RunEstimator(slots, type_bits));
    AppendPackUnchecked(fmt, slots, type_bits);
  }

  void AppendPackUnchecked(StringSlice fmt, PackedValue *args, uint64_t type_bits) {
#ifdef PDP_ENABLE_ASSERT
    StringSlice original_fmt = fmt;
#endif
    while (type_bits > 0) {
      const char *it = fmt.MemChar('{');
      if (PDP_UNLIKELY(!it)) {
#ifdef PDP_ENABLE_ASSERT
        OnAssertFailed("Extra arguments for format", original_fmt.Begin(), original_fmt.Size());
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
      OnAssertFailed("Insufficient arguments for format", original_fmt.Begin(),
                     original_fmt.Size());
    }
#endif
    AppendUnchecked(fmt);
  }

  void ReserveFor(size_t new_elems) {
    pdp_assert(max_capacity - new_elems >= Size());
    const char *__restrict__ new_limit = end + new_elems;
    if (PDP_UNLIKELY(new_limit > limit)) {
      GrowExtra(new_limit - limit);
    }
  }

 private:
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
      default:
        pdp_assert(false);
        return 0;
    }
  }

  void GrowExtra(const size_t extra_limit) {
    size_t size = Size();
    size_t capacity = Capacity();

    const size_t half_capacity = capacity / 2;
    const size_t grow_capacity = half_capacity > extra_limit ? half_capacity : extra_limit;

    [[maybe_unused]]
    const bool ok_limit = max_capacity - grow_capacity >= capacity;
    pdp_assert(ok_limit);
    capacity += grow_capacity;

    if (PDP_LIKELY(begin != buffer)) {
      begin = Reallocate<char>(allocator, begin, capacity);
    } else {
      begin = Allocate<char>(allocator, capacity);
      memcpy(begin, buffer, sizeof(buffer));
    }
    end = begin + size;
    limit = begin + capacity;
    pdp_assert(begin);
  }

  static constexpr const size_t max_capacity = 1_GB;

  char buffer[256];
  char *__restrict__ begin;
  char *__restrict__ end;
  const char *__restrict__ limit;

  Alloc allocator;
};

};  // namespace pdp

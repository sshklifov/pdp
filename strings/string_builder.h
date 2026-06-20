#pragma once

#include "formatter.h"
#include "string_slice.h"

#include "core/internals.h"
#include "data/allocator.h"

namespace pdp {

template <typename T, size_t N, typename Alloc>
struct SmallBufferStorage {
  SmallBufferStorage(Alloc a = Alloc())
      : begin(reinterpret_cast<T *>(storage)),
        end(reinterpret_cast<T *>(storage)),
        limit(reinterpret_cast<T *>(storage) + N),
        allocator(a) {
#ifdef PDP_ENABLE_ZERO_INITIALIZE
    if (std::is_trivially_constructible_v<T>) {
      memset(storage, 0, sizeof(storage));
    }
#endif
  }

  ~SmallBufferStorage() {
    Clear();
    if (PDP_UNLIKELY(reinterpret_cast<byte *>(begin) != storage)) {
      Deallocate<T>(allocator, begin);
    }
  }

  bool Empty() const { return begin == end; }
  size_t Length() const { return end - begin; }
  size_t Size() const { return Length(); }
  size_t Capacity() const { return limit - begin; }

  T *Begin() { return this->begin; }
  const T *Begin() const { return this->begin; }

  const T *Data() const { return this->begin; }

  T *End() { return this->end; }
  const T *End() const { return this->end; }

  T operator[](size_t pos) const {
    pdp_assert(begin + pos < end);
    return begin[pos];
  }

  T &operator[](size_t pos) {
    pdp_assert(begin + pos < end);
    return begin[pos];
  }

  void ReserveFor(size_t new_elems) {
    auto *required_limit = end + new_elems;
    if (PDP_UNLIKELY(required_limit > limit)) {
      GrowExtra(required_limit - limit);
    }
  }

  void Clear() {
    if constexpr (!std::is_trivially_destructible_v<T>) {
      for (auto it = begin; it < end; ++it) {
        it->~T();
      }
    }
    end = begin;
  }

 private:
  void GrowExtra(const size_t extra_capacity) {
    size_t size = Size();
    size_t capacity = Capacity();
    const size_t half_capacity = capacity / 2;
    const size_t grow_capacity = half_capacity > extra_capacity ? half_capacity : extra_capacity;

    capacity += grow_capacity;

    if (PDP_LIKELY(reinterpret_cast<byte *>(begin) != storage)) {
      begin = Reallocate<T>(allocator, begin, capacity);
    } else {
      begin = Allocate<T>(allocator, capacity);
      memcpy(begin, storage, sizeof(storage));
    }
    pdp_assert(begin);
    end = begin + size;
    limit = begin + capacity;
  }

 protected:
  // TODO max capacity is so stupid, use DefaultAllocator for that shi.

  alignas(T) byte storage[sizeof(T) * N];

  T *__restrict__ begin;
  T *__restrict__ end;
  const T *__restrict__ limit;

  Alloc allocator;
};

template <typename T, size_t N>
struct CanReallocate<SmallBufferStorage<T, N, DefaultAllocator>> : CanReallocate<T> {};

template <typename Alloc>
using SmallCharBuffer = SmallBufferStorage<char, 256, Alloc>;

template <typename Alloc = DefaultAllocator>
struct StringBuilder : public SmallCharBuffer<Alloc> {
  using SmallCharBuffer<Alloc>::begin;
  using SmallCharBuffer<Alloc>::end;
  using SmallCharBuffer<Alloc>::limit;

  StringSlice ToSlice() const { return StringSlice(begin, end); }

  void Truncate(size_t old_size) {
    auto new_end = this->begin + old_size;
    pdp_assert(new_end < this->end);
    this->end = new_end;
  }

  // Unsafe Append methods.

  template <typename T>
  void AppendUnchecked(T &&value) {
    Formatter fmt(end, limit);
    fmt.AppendUnchecked(std::forward<T>(value));
    end = fmt.End();
  }

  // Super-super unsafe Append methods.

  char *AppendUninitialized(size_t n) {
    this->ReserveFor(n);
    auto result = this->end;
    this->end += n;
    return result;
  }

  // Safe Append version.
  template <typename T, size_t U = EstimateSizeV<std::decay_t<T>>>
  void Append(T &&value) {
    this->ReserveFor(U);
    AppendUnchecked(std::forward<T>(value));
  }

  void Append(const char *str) { Append(StringSlice(str)); }

  void Append(StringSlice str) {
    this->ReserveFor(str.Size());
    AppendUnchecked(str);
  }

  void Append(Hex64 hex) {
    this->ReserveFor(EstimateSizeV<void *>);
    AppendUnchecked(BitCast<void *>(hex.value));
  }

  // Append variadic arguments.

  template <typename... Args>
  void AppendFormat(const StringSlice &fmt, Args &&...args) {
    auto packed_args = MakePackedArgs(std::forward<Args>(args)...);
    AppendPack(fmt, packed_args.slots, packed_args.type_bits);
  }

  void AppendPack(const StringSlice &fmt, PackedValue *slots, uint64_t type_bits) {
    this->ReserveFor(fmt.Size() + RunEstimator(slots, type_bits));
    AppendPackUnchecked(fmt, slots, type_bits);
  }

  void AppendPackUnchecked(const StringSlice &fmt, PackedValue *slots, uint64_t type_bits) {
    Formatter formatter(end, limit);
    formatter.AppendPackUnchecked(fmt, slots, type_bits);
    end = formatter.End();
  }
};

};  // namespace pdp

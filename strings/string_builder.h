#pragma once

#include "formatter.h"
#include "string_slice.h"

#include "core/internals.h"
#include "data/allocator.h"

namespace pdp {

template <typename T, typename Alloc>
struct SmallBufferStorage {
  SmallBufferStorage() : begin(buffer), end(buffer), limit(buffer + sizeof(buffer)) {
#ifdef PDP_ENABLE_ZERO_INITIALIZE
    memset(buffer, 0, sizeof(buffer));
#endif
  }

  ~SmallBufferStorage() {
    if (PDP_UNLIKELY(begin != buffer)) {
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

  void ReserveFor(size_t new_elems) {
    pdp_assert(max_capacity - new_elems >= Size());
    const T *__restrict__ new_limit = end + new_elems;
    if (PDP_UNLIKELY(new_limit > limit)) {
      GrowExtra(new_limit - limit);
    }
  }

  void Clear() { end = begin; }

 private:
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
      begin = Reallocate<T>(allocator, begin, capacity);
    } else {
      begin = Allocate<T>(allocator, capacity);
      memcpy(begin, buffer, sizeof(buffer));
    }
    end = begin + size;
    limit = begin + capacity;
    pdp_assert(begin);
  }

 protected:
  static constexpr size_t max_capacity = 256_MB;

  T buffer[256];
  T *__restrict__ begin;
  T *__restrict__ end;
  const T *__restrict__ limit;

  Alloc allocator;
};

template <typename Alloc = DefaultAllocator>
struct StringBuilder : public SmallBufferStorage<char, Alloc> {
  using SmallBufferStorage<char, Alloc>::begin;
  using SmallBufferStorage<char, Alloc>::end;
  using SmallBufferStorage<char, Alloc>::limit;

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

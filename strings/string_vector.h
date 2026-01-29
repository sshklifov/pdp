#pragma once

#include "formatter.h"
#include "string_slice.h"

#include "data/allocator.h"
#include "data/vector.h"

namespace pdp {

namespace impl {

template <char Delim>
struct _SplitView {
  struct Iterator {
    Iterator() : begin(nullptr), delim(nullptr), end(nullptr) {}

    explicit Iterator(const char *b, const char *e) : begin(b), end(e) {
      pdp_assert(end - begin > 0);
      delim = (const char *)memchr(begin, Delim, end - begin);
      if (PDP_UNLIKELY(!delim)) {
        delim = end;
      }
    }

    static Iterator End() { return Iterator(); }

    StringSlice operator*() const {
      pdp_assert(delim);
      return StringSlice(begin, delim);
    }

    Iterator &operator++() {
      pdp_assert(delim);
      begin = delim;
      if (PDP_UNLIKELY(begin == end)) {
        delim = nullptr;
        return (*this);
      }
      begin++;
      delim = (const char *)memchr(begin, Delim, end - begin);
      if (PDP_UNLIKELY(delim == nullptr)) {
        delim = end;
      }
      return (*this);
    }

    bool operator==(const Iterator &rhs) const { return delim == rhs.delim; }

    bool operator!=(const Iterator &rhs) const { return delim != rhs.delim; }

   private:
    const char *begin;
    const char *delim;
    const char *end;
  };

  _SplitView(const char *b, const char *e) : fwd_b(b), fwd_e(e) {}

  Iterator begin() { return Iterator(fwd_b, fwd_e); }

  Iterator end() { return Iterator::End(); }

 private:
  const char *fwd_b;
  const char *fwd_e;
};

}  // namespace impl

using NullTerminateSplit = impl::_SplitView<'\0'>;
using NewlineSplit = impl::_SplitView<'\n'>;

struct StringVector : public Vector<char, DefaultAllocator> {
  using Vector<char>::Vector;

  void MoveEnd(size_t n) {
    pdp_assert(size + n <= capacity);
    size += n;
  }

  void Append(char c) { (*this) += c; }

  size_t AppendPack(const StringSlice &fmt, PackedValue *args, uint64_t type_bits) {
    ReserveFor(RunEstimator(args, type_bits));
    Formatter formatter(ptr + size, ptr + capacity);
    formatter.AppendPackUnchecked(fmt, args, type_bits);
    const auto num_formatted = formatter.End() - End();
    size = formatter.End() - Begin();
    return num_formatted;
  }

  size_t Free() const { return capacity - size; }

  void MemCopyUnchecked(const char *src, size_t n) {
    pdp_assert(Free() >= n);
    memcpy(End(), src, n);
    size += n;
  }

  void MemCopy(const char *src, size_t n) {
    ReserveFor(n);
    MemCopyUnchecked(src, n);
  }

  void MemCopy(const StringSlice &s) { MemCopy(s.Begin(), s.Size()); }

  NullTerminateSplit SplitByNull() const { return NullTerminateSplit(Begin(), End()); }

  NewlineSplit SplitByNewline() const { return NewlineSplit(Begin(), End()); }
};

template <>
struct CanReallocate<StringVector> : std::true_type {};

}  // namespace pdp

#pragma once

#include "external/ankerl_hash.h"
#include "external/emhash8.h"
#include "strings/fixed_string.h"

namespace pdp {

struct FileLineView {
  FileLineView(const StringSlice &f, int l) : fullname(f), lnum(l) {}

  StringSlice fullname;
  int lnum;
};

struct FileLine {
  FileLine(const StringSlice &f, int l) : fullname(f), lnum(l) {}
  FileLine(FixedString &&f, int l) : fullname(std::move(f)), lnum(l) {}
  explicit FileLine(const FileLineView &v) : fullname(v.fullname), lnum(v.lnum) {}

  explicit operator FileLineView() const { return FileLineView(fullname.ToSlice(), lnum); }

  bool operator==(const FileLine &rhs) const {
    return lnum == rhs.lnum && fullname == rhs.fullname;
  }

  bool operator==(const FileLineView &rhs) const {
    return lnum == rhs.lnum && fullname == rhs.fullname;
  }

  FixedString fullname;
  int lnum;
};

template <>
struct CanReallocate<FileLine> : std::true_type {};

template <>
struct Hash<FileLineView> {
  uint64_t operator()(const FileLineView &key) const {
    uint64_t seed = ankerl::unordered_dense::hash(key.fullname.Begin(), key.fullname.Size());
    return ankerl::unordered_dense::mix(key.lnum, seed);
  }
};

template <>
struct Hash<FileLine> {
  uint64_t operator()(const FileLine &key) const {
    uint64_t seed = ankerl::unordered_dense::hash(key.fullname.Begin(), key.fullname.Size());
    return ankerl::unordered_dense::mix(key.lnum, seed);
  }
};

template <typename T, size_t N>
struct SmallStack : public SmallBufferStorage<T, N, DefaultAllocator> {
  using SmallBufferStorage<T, N, DefaultAllocator>::SmallBufferStorage;

  template <typename... Args>
  void Emplace(Args &&...args) {
    this->ReserveFor(1);
    new (this->End()) T(std::forward<Args>(args)...);
  }

  T &Top() {
    pdp_assert(!this->Empty());
    return *(this->end - 1);
  }

  const T &Top() const {
    pdp_assert(!this->Empty());
    return *(this->end - 1);
  }

  void Pop() {
    pdp_assert(!this->Empty());
    --this->end;
    if constexpr (!std::is_trivially_destructible_v<T>) {
      this->end->~T();
    }
  }
};

template <typename T, size_t N>
struct CanReallocate<SmallStack<T, N>> : CanReallocate<T> {};

};  // namespace pdp

namespace emhash8 {

template <typename V>
using FileLineMap = emhash8::Map3<pdp::FileLineView, pdp::FileLine, V>;

}  // namespace emhash8

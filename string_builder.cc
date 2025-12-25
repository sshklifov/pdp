#include "string_builder.h"

#include <cstdlib>
#include <cstring>
#include <exception>

namespace pdp {

size_t EstimateSize(const StringView &s) { return s.Size(); }

size_t EstimateSize(char c) { return 1; }

size_t EstimateSize(void *p) {
  // Max decimal digits for 64-bit + "0x" prefix
  return std::numeric_limits<size_t>::digits10 + 2;
}

Buffer::Buffer(size_t cap) noexcept
    : ptr(static_cast<char *>(malloc(cap))), size(0), capacity(cap) {
  assert(cap > 0);
}

Buffer::Buffer(Buffer &&other) : ptr(other.ptr), size(other.size), capacity(other.capacity) {
  other.ptr = nullptr;
}

Buffer &Buffer::operator=(Buffer &&other) {
  if (this != &other) {
    ptr = other.ptr;
    size = other.size;
    capacity = other.capacity;
    other.ptr = nullptr;
  }
  return (*this);
}

Buffer::~Buffer() { free(ptr); }

const char *Buffer::Data() const { return ptr; }

char *Buffer::Data() { return ptr; }

const char *Buffer::Begin() const { return ptr; }

const char *Buffer::End() const { return ptr + size; }

char *Buffer::Begin() { return ptr; }

char *Buffer::End() { return ptr + size; }

StringView Buffer::Substr(size_t pos) const { return StringView(ptr + pos, size - pos); }

StringView Buffer::Substr(size_t pos, size_t n) const { return StringView(ptr + pos, n); }

StringView Buffer::Substr(const char *it) const { return StringView(it, End()); }

StringView Buffer::ViewOnly() const { return StringView(ptr, size); }

bool Buffer::Empty() const { return size == 0; }

size_t Buffer::Size() const { return size; }

size_t Buffer::Length() const { return size; }

size_t Buffer::Capacity() const { return capacity; }

bool Buffer::operator==(const StringView &other) const {
  if (Size() != other.Size()) {
    return false;
  }
  return strncmp(Begin(), other.Begin(), Size()) == 0;
}

bool Buffer::operator!=(const StringView &other) const { return !(*this == other); }

void Buffer::Clear() { size = 0; }

void Buffer::Reserve(size_t new_capacity) {
  if (new_capacity > capacity) {
    GrowExtra(new_capacity - capacity);
  }
}

void Buffer::Grow() { GrowExtra(1); }

Buffer &Buffer::operator+=(char c) {
  Append(c);
  return (*this);
}

void Buffer::Append(const char *begin, const char *end) { Append(StringView(begin, end)); }

void Buffer::Append(char c, size_t n) {
  Reserve(size + n);
  memset(ptr + size, c, n);
  size += n;
}

char &Buffer::operator[](size_t index) {
  assert(index < Size());
  return ptr[index];
}

const char &Buffer::operator[](size_t index) const {
  assert(index < Size());
  return ptr[index];
}

void Buffer::AppendfUnchecked(const StringView &fmt) {
#ifndef NDEBUG
  // TODO
#endif

  const bool more_work = !fmt.Empty();
  if (more_work) {
    Append(fmt);
  }
}

void Buffer::AppendUnchecked(char c) {
  assert(size + 1 <= capacity);
  ptr[size++] = c;
}

void Buffer::AppendUnchecked(const StringView &s) {
  assert(size + s.Size() <= capacity);
  memcpy(ptr + size, s.Begin(), s.Size());
  size += s.Size();
}

void Buffer::AppendUnchecked(void *p) {
  AppendUnchecked('0');
  AppendUnchecked('x');
  AppendUnchecked(reinterpret_cast<size_t>(p));
}

void Buffer::GrowExtra(const size_t req_capacity) {
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

StringBuilder::StringBuilder() : Buffer(default_buffer_capacity) {}

};  // namespace pdp

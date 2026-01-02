#pragma once

#include "tracing/trace_likely.h"
#include "data/stack.h"
#include "strings/string_slice.h"

#include <cstdint>

namespace pdp {

bool IsIdentifier(char c);

bool IsBracketMatch(char open, char close);

// TODO
// char *ConvertCString(char *__restrict dst, const char *__restrict src_begin,
//                      const char *__restrict src_end);

// TODO: Gold but not dug yet
// inline char Previous(const char *it, unsigned n = 1) { return it[-n]; }

struct FirstPass {
  friend struct SecondPass;

  FirstPass(const StringSlice &s);

  bool Parse();

 private:
  bool ReportError(const StringSlice &msg);
  void PushSizeOnStack(uint32_t num_elements);
  void AccumulateBytes();

  bool ParseResult();
  bool ParseValue();
  bool ParseString();
  bool ParseListOrTuple();
  bool ParseResultOrValue();

  struct Record {
    uint32_t num_elements;
    uint32_t total_string_size;
  };

  StringSlice input;
  Stack<uint32_t> nesting_stack;
  Stack<Record> sizes_stack;

  uint32_t total_bytes;
};

struct ArenaTraits {
  static constexpr uint32_t AlignUp(uint32_t bytes) {
    return bytes = (bytes + alignment - 1) & ~(alignment - 1);
  }

  static constexpr const uint32_t alignment = 8;
};

template <typename Alloc>
struct Arena : public ArenaTraits {
  Arena(size_t cap) {
    chunk = static_cast<unsigned char *>(allocator.AllocateRaw(cap));
    pdp_assert(chunk);
    pdp_assert(reinterpret_cast<uint64_t>(chunk) % alignment == 0);
    head = chunk;
#ifdef PDP_ENABLE_ASSERT
    capacity = cap;
#endif
  }

  ~Arena() {
    pdp_assert(chunk);
    allocator.DeallocateRaw(chunk);
  }

  void *Allocate(uint32_t bytes) { return AllocateUnchecked(AlignUp(bytes)); }

  void *AllocateUnchecked(uint32_t bytes) {
    pdp_assert(bytes > 0);
    pdp_assert(bytes % alignment == 0);
    pdp_assert(uint32_t(head - chunk) + bytes <= capacity);
    void *ptr = head;
    head += bytes;
    return ptr;
  }

  void *AllocateOrNull(uint32_t bytes) {
    if (PDP_TRACE_LIKELY(bytes > 0)) {
      return Allocate(bytes);
    } else {
      return nullptr;
    }
  }

 private:
  unsigned char *chunk;
  unsigned char *head;
#ifdef PDP_ENABLE_ASSERT
  size_t capacity;
#endif

  Alloc allocator;
};

struct ExprBase {
  enum Kind { kString, kList, kTuple };

  uint8_t kind;
  uint8_t unused[3];
  uint32_t size;
};

struct ExprString : public ExprBase {
  char payload[0];
};

static_assert(sizeof(ExprString) == 8 && alignof(ExprString) <= 8);

struct ExprList : public ExprBase {
  char payload[0];
};

static_assert(sizeof(ExprList) == 8 && alignof(ExprList) <= 8);

// null terminated table here
struct ExprTuple : public ExprBase {
  struct Result {
    const char *key;
    ExprBase *value;
  };

  uint32_t *hashes;
  Result *results;
  char payload[0];
};

static_assert(sizeof(ExprTuple) == 24 && alignof(ExprTuple) <= 8);

struct SecondPass {
  SecondPass(const StringSlice &s, FirstPass &first_pass);

  ExprBase *Parse();

 private:
  ExprBase *ReportError(const StringSlice &msg);
  ExprBase *CreateListOrTuple();

  ExprBase *ParseResult();
  ExprBase *ParseValue();
  ExprBase *ParseString();
  ExprBase *ParseListOrTuple();
  ExprBase *ParseResultOrValue();

  struct ExprRecord {
    ExprBase *expr;
    union {
      ExprBase **list_members;
      ExprTuple::Result *tuple_members;
    };
    char *string_table_ptr;
    uint32_t *hash_table_ptr;
#ifdef PDP_ENABLE_ASSERT
    void *record_end;
#endif
  };

  StringSlice input;
  Stack<FirstPass::Record> first_pass_stack;
  size_t first_pass_marker;
  Arena<DefaultAllocator> arena;
  Stack<ExprRecord> second_pass_stack;
};

}  // namespace pdp

#pragma once

#include "arena.h"
#include "expr.h"

#include "data/stack.h"
#include "strings/string_slice.h"

#include <cstdint>

namespace pdp {

bool IsIdentifier(char c);

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

#pragma once

#include "data/unique_ptr.h"
#include "expr.h"

#include "data/arena.h"
#include "data/stack.h"
#include "strings/string_slice.h"

#include <cstdint>

namespace pdp {

bool IsMiIdentifier(char c);

char ReverseEscapeCharacter(char c);

struct MiFirstPass {
  friend struct MiSecondPass;

  MiFirstPass(const StringSlice &s);

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

  struct MiRecord {
    uint32_t num_elements;
    uint32_t total_string_size;
  };

  StringSlice input;
  Stack<uint32_t> nesting_stack;
  Stack<MiRecord> sizes_stack;

  uint32_t total_bytes;
};

struct MiSecondPass {
  MiSecondPass(const StringSlice &s, MiFirstPass &first_pass);

  UniquePtr<ExprBase> Parse();

 private:
  ExprBase *ReportError(const StringSlice &msg);
  ExprBase *CreateListOrTuple();

  ExprBase *ParseResult();
  ExprBase *ParseValue();
  ExprBase *ParseString();
  ExprBase *ParseListOrTuple();
  ExprBase *ParseResultOrValue();

  struct MiRecord {
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
  Stack<MiFirstPass::MiRecord> first_pass_stack;
  size_t first_pass_marker;
  Arena<DefaultAllocator> arena;
  Stack<MiRecord> second_pass_stack;
};

}  // namespace pdp

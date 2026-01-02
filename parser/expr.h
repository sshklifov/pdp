#pragma once

#include <cstdint>

namespace pdp {

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

}  // namespace pdp

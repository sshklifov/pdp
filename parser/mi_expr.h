#pragma once

#include "strings/string_slice.h"

#include <cstdint>

namespace pdp {

struct MiExprBase {
  enum Kind { kString, kList, kTuple };

  uint8_t kind;
  uint8_t unused[3];
  uint32_t size;
};

struct MiExprString : public MiExprBase {
  char payload[0];
};

static_assert(sizeof(MiExprString) == 8 && alignof(MiExprString) <= 8);

struct MiExprList : public MiExprBase {
  char payload[0];
};

static_assert(sizeof(MiExprList) == 8 && alignof(MiExprList) <= 8);

struct MiExprTuple : public MiExprBase {
  struct Result {
    const char *key;
    MiExprBase *value;
  };

  uint32_t *hashes;
  Result *results;
  char payload[0];
};

static_assert(sizeof(MiExprTuple) == 24 && alignof(MiExprTuple) <= 8);

struct MiNiceExpr {
  MiNiceExpr(const MiExprBase *expr);

  uint32_t Count() const;

  MiNiceExpr operator[](uint32_t index);
  MiNiceExpr operator[](const StringSlice &key);

  StringSlice StringOr(const StringSlice &alternative) const;
  int32_t NumberOr(int32_t alternative) const;

  void Print();

 private:
  const MiExprBase *expr;
};

}  // namespace pdp

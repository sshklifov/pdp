#pragma once

#include "strings/string_slice.h"

#include <cstdint>

namespace pdp {

struct ExprBase {
  enum Kind { kNull, kInt, kString, kList, kTuple, kMap };

  uint8_t kind;
  uint8_t unused[3];
  uint32_t size;
};

struct ExprInt : public ExprBase {
  int64_t value;
};

static_assert(sizeof(ExprInt) == 16 && alignof(ExprInt) <= 8);

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

struct ExprMap : public ExprBase {
  struct Pair {
    ExprBase *key;
    ExprBase *value;
  };

  // TODO
  // uint32_t *hashes;
  char payload[0];
};

static_assert(sizeof(ExprMap) == 8 && alignof(ExprMap) <= 8);

struct ExprView {
  ExprView(const ExprBase *expr);

  uint32_t Count() const;

  ExprView operator[](uint32_t index);
  ExprView operator[](const StringSlice &key);

  StringSlice StringOr(const StringSlice &alternative) const;
  int32_t NumberOr(int32_t alternative) const;

  void Print();

 private:
  const ExprBase *expr;
};

}  // namespace pdp

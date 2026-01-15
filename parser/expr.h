#pragma once

#include "strings/string_builder.h"
#include "strings/string_slice.h"

#include <cstdint>

namespace pdp {

struct ExprBase {
  enum Kind { kNull, kInt, kString, kList, kTuple, kMap };

  uint8_t kind;
  uint8_t unused[3];
  uint32_t size;
};

inline const char *ExprKindToString(uint8_t kind) {
  switch (kind) {
    case ExprBase::kNull:
      return "Null";
    case ExprBase::kInt:
      return "Integer";
    case ExprBase::kString:
      return "String";
    case ExprBase::kList:
      return "List";
    case ExprBase::kTuple:
      return "Tuple";
    case ExprBase::kMap:
      return "Map";
    default:
      pdp_assert(false);
      return "???";
  }
}

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
  uint32_t *hashes;
  Pair *pairs;
  char payload[0];
};

static_assert(sizeof(ExprMap) == 24 && alignof(ExprMap) <= 8);

struct ExprBaseView {
  ExprBaseView(const ExprBase *expr);

  uint32_t Count() const;

  operator bool() const;

  void ToJson(StringBuilder<> &out) const;

  static StringSlice GetStringUnchecked(const ExprBase *e);
  static int64_t GetIntegerUnchecked(const ExprBase *e);
  static const ExprBase *const *GetListUnchecked(const ExprBase *e);

 protected:
  StringSlice AsStringUnchecked() const;
  int64_t AsIntegerUnchecked() const;
  const ExprBase *const *AsListUnchecked() const;
  const ExprMap *AsMapUnchecked() const;
  const ExprTuple *AsTupleUnchecked() const;

  const ExprBase *expr;
};

struct LooseTypedView : public ExprBaseView {
  LooseTypedView(const ExprBase *expr);

  LooseTypedView operator[](const StringSlice &key) const;
  LooseTypedView operator[](const char *key) const;

  LooseTypedView operator[](uint32_t index) const;

  int64_t AsInteger() const;
  StringSlice AsString() const;

  bool operator==(const StringSlice &str) const;
  bool operator!=(const StringSlice &str) const;
};

}  // namespace pdp

#include "expr.h"

#include "external/ankerl_hash.h"

namespace pdp {

ExprBaseView::operator bool() const { return expr != nullptr; }

uint32_t ExprBaseView::Count() const {
  switch (expr->kind) {
    case ExprBase::kTuple:
    case ExprBase::kList:
    case ExprBase::kMap:
      return expr->size;
    default:
      return 0;
  }
}

ExprBaseView::ExprBaseView(const ExprBase *expr) : expr(expr) {}

StringSlice ExprBaseView::GetStringUnchecked(const ExprBase *e) {
  pdp_assert(e->kind == ExprBase::kString);
  const char *data = (const char *)e + sizeof(ExprString);
  return StringSlice(data, e->size);
}

int64_t ExprBaseView::GetIntegerUnchecked(const ExprBase *e) {
  pdp_assert(e->kind == ExprBase::kInt);
  auto integer = static_cast<const ExprInt *>(e);
  return integer->value;
}

const ExprBase *const *ExprBaseView::GetListUnchecked(const ExprBase *e) {
  pdp_assert(e->kind == ExprBase::kList);
  return reinterpret_cast<ExprBase **>((char *)e + sizeof(ExprList));
}

StringSlice ExprBaseView::AsStringUnchecked() const { return GetStringUnchecked(expr); }

int64_t ExprBaseView::AsIntegerUnchecked() const { return GetIntegerUnchecked(expr); }

const ExprBase *const *ExprBaseView::AsListUnchecked() const { return GetListUnchecked(expr); }

const ExprMap *ExprBaseView::AsMapUnchecked() const {
  pdp_assert(expr->kind == ExprBase::kMap);
  return static_cast<const ExprMap *>(expr);
}

const ExprTuple *ExprBaseView::AsTupleUnchecked() const {
  pdp_assert(expr->kind == ExprBase::kTuple);
  return static_cast<const ExprTuple *>(expr);
}

static StringSlice KeyOr(const ExprBase *expr, const StringSlice &alternative) {
  if (PDP_UNLIKELY(expr->kind != ExprBase::kString)) {
    return alternative;
  }
  return ExprBaseView::GetStringUnchecked(expr);
}

static void RecursiveToJson(const ExprBase *expr, StringBuilder<DefaultAllocator> &builder) {
  if (expr->kind == ExprBase::kString) {
    StringSlice s = ExprBaseView::GetStringUnchecked(expr);
    builder.Append('"');
    builder.Append(s);
    builder.Append('"');
  } else if (expr->kind == ExprBase::kList) {
    if (expr->size == 0) {
      builder.Append("[]");
    } else {
      builder.Append('[');
      auto elements = ExprBaseView::GetListUnchecked(expr);
      RecursiveToJson(elements[0], builder);
      for (size_t i = 1; i < expr->size; ++i) {
        builder.Append(", ");
        RecursiveToJson(elements[i], builder);
      }
      builder.Append(']');
    }
  } else if (expr->kind == ExprBase::kTuple) {
    if (expr->size == 0) {
      builder.Append("{}");
    } else {
      const ExprTuple *tuple = static_cast<const ExprTuple *>(expr);
      const ExprTuple::Result *results = tuple->results;
      builder.AppendFormat("{\"{}\":", StringSlice(results[0].key));
      RecursiveToJson(results[0].value, builder);
      for (size_t i = 1; i < expr->size; ++i) {
        builder.AppendFormat(",\"{}\":", StringSlice(results[i].key));
        RecursiveToJson(results[i].value, builder);
      }
      builder.Append('}');
    }
  } else if (expr->kind == ExprBase::kMap) {
    if (expr->size == 0) {
      builder.Append("{}");
    } else {
      const ExprMap *map = static_cast<const ExprMap *>(expr);
      const ExprMap::Pair *pairs = map->pairs;
      builder.AppendFormat("{\"{}\":", KeyOr(pairs[0].key, "??"));
      RecursiveToJson(pairs[0].value, builder);
      for (size_t i = 1; i < expr->size; ++i) {
        builder.AppendFormat(",\"{}\":", KeyOr(pairs[i].key, "??"));
        RecursiveToJson(pairs[i].value, builder);
      }
      builder.Append('}');
    }
  } else if (expr->kind == ExprBase::kInt) {
    auto value = ExprBaseView::GetIntegerUnchecked(expr);
    builder.Append(value);
  } else if (expr->kind == ExprBase::kNull) {
    builder.Append("null");
  } else {
    pdp_assert(false);
  }
}

void ExprBaseView::ToJson(StringBuilder<> &out) const { RecursiveToJson(expr, out); }

GdbExprView::GdbExprView(const ExprBase *expr) : ExprBaseView(expr) {}

GdbExprView::GdbExprView(const UniquePtr<ExprBase> &expr) : ExprBaseView(expr.Get()) {}

GdbExprView GdbExprView::operator[](const StringSlice &key) const {
  RequireNotNull();
  if (PDP_LIKELY(expr->kind == ExprBase::kTuple)) {
    const ExprTuple *tuple = AsTupleUnchecked();
    uint32_t hash = ankerl::unordered_dense::hash(key.Begin(), key.Size());
    for (uint32_t i = 0; i < tuple->size; ++i) {
      if (PDP_UNLIKELY(tuple->hashes[i] == hash)) {
        const ExprTuple::Result *result = tuple->results + i;
        if (PDP_LIKELY(key == result->key)) {
          return result->value;
        }
      }
    }
  }
  return nullptr;
}

GdbExprView GdbExprView::operator[](const char *key) const {
  return (*this)[StringSlice(key)];
}

GdbExprView GdbExprView::operator[](uint32_t index) const {
  RequireNotNull();
  if (PDP_LIKELY(expr->kind == ExprBase::kList)) {
    if (PDP_LIKELY(index < expr->size)) {
      auto elements = AsListUnchecked();
      return elements[index];
    }
  }
  return nullptr;
}

int64_t GdbExprView::RequireInt() const {
  RequireNotNull();
  pdp_assert(expr->kind != ExprBase::kInt);
  if (PDP_LIKELY(expr->kind == ExprBase::kString)) {
    const char *str = AsStringUnchecked().Begin();
    const bool is_negative = (*str == '-');
    str += is_negative;

    int32_t res = 0;
    for (size_t i = 0; i < expr->size; ++i) {
      if (PDP_UNLIKELY(str[i] < '0' || str[i] > '9')) {
        PDP_UNREACHABLE("Contract violation: integer access failed!");
      }
      res *= 10;
      res += str[i] - '0';
    }
    if (is_negative) {
      return -res;
    } else {
      return res;
    }
  }
  PDP_UNREACHABLE("Contract violation: integer access failed!");
}

StringSlice GdbExprView::RequireStr() const {
  RequireNotNull();
  if (PDP_LIKELY(expr->kind == ExprBase::kString)) {
    return AsStringUnchecked();
  }
  PDP_UNREACHABLE("Contract violation: string access failed!");
}

StringSlice GdbExprView::StrOr(const StringSlice &alternative) const {
  if (PDP_LIKELY(expr && expr->kind == ExprBase::kString)) {
    return AsStringUnchecked();
  } else {
    return alternative;
  }
}

bool GdbExprView::operator==(const StringSlice &str) const { return RequireStr() == str; }

bool GdbExprView::operator!=(const StringSlice &str) const { return !(*this == str); }

void GdbExprView::RequireNotNull() const {
  if (PDP_UNLIKELY(expr == nullptr)) {
    PDP_UNREACHABLE("Contract violation: null expression");
  }
}

StrongTypedView::StrongTypedView(const ExprBase *expr) : ExprBaseView(expr) {}

StrongTypedView StrongTypedView::operator[](const StringSlice &key) const {
  if (PDP_LIKELY(expr->kind == ExprBase::kMap)) {
    auto map = AsMapUnchecked();
    uint32_t hash = ankerl::unordered_dense::hash(key.Begin(), key.Size());
    for (uint32_t i = 0; i < map->size; ++i) {
      if (PDP_UNLIKELY(map->hashes[i] == hash)) {
        auto pair = map->pairs + i;
        StrongTypedView pair_key_view(pair->key);
        if (PDP_LIKELY(pair_key_view == key)) {
          return pair->value;
        }
      }
    }
  }
  return nullptr;
}

StrongTypedView StrongTypedView::operator[](const char *key) const {
  return (*this)[StringSlice(key)];
}

StrongTypedView StrongTypedView::operator[](uint32_t index) const {
  if (PDP_LIKELY(expr->kind == ExprBase::kList)) {
    if (PDP_LIKELY(index < expr->size)) {
      auto elements = AsListUnchecked();
      return elements[index];
    }
  }
  return nullptr;
}

int64_t StrongTypedView::AsInteger() const {
  if (PDP_LIKELY(expr->kind == ExprBase::kInt)) {
    return AsIntegerUnchecked();
  }
  PDP_UNREACHABLE("Contract violation: integer access failed!");
}

StringSlice StrongTypedView::AsString() const {
  if (PDP_LIKELY(expr->kind == ExprBase::kString)) {
    return AsStringUnchecked();
  }
  PDP_UNREACHABLE("Contract violation: string access failed!");
}

bool StrongTypedView::operator==(const StringSlice &str) const { return AsString() == str; }

bool StrongTypedView::operator!=(const StringSlice &str) const { return !(*this == str); }

}  // namespace pdp

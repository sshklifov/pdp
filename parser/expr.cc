#include "expr.h"

#include "core/log.h"
#include "external/ankerl_hash.h"

namespace pdp {

uint32_t ExprView::Count() const {
  switch (expr->kind) {
    case ExprBase::kTuple:
    case ExprBase::kList:
      return expr->size;
    default:
      return 0;
  }
}

ExprView::ExprView(const ExprBase *expr) : expr(expr) {}

ExprView ExprView::operator[](uint32_t index) {
  if (PDP_UNLIKELY(expr->kind != ExprBase::kList)) {
    pdp_warning("List access on non-list expression");
    return nullptr;
  }

  if (PDP_UNLIKELY(index >= expr->size)) {
    pdp_warning("List access out of range");
    return nullptr;
  }
  ExprBase **elements = reinterpret_cast<ExprBase **>((char *)expr + sizeof(ExprList));
  return elements[index];
}

ExprView ExprView::operator[](const StringSlice &key) {
  if (PDP_UNLIKELY(expr->kind != ExprBase::kTuple)) {
    pdp_warning("Tuple access on non-tuple expression");
    return nullptr;
  }

  const ExprTuple *tuple = static_cast<const ExprTuple *>(expr);
  uint32_t num_elements = tuple->size;
  uint32_t hash = ankerl::unordered_dense::hash(key.Begin(), key.Size());
  for (uint32_t i = 0; i < num_elements; ++i) {
    if (PDP_UNLIKELY(tuple->hashes[i] == hash)) {
      ExprTuple::Result *result = tuple->results + i;
      if (PDP_LIKELY(key == result->key)) {
        return result->value;
      }
    }
  }
  return nullptr;
}

StringSlice ExprView::StringOr(const StringSlice &alternative) const {
  if (PDP_UNLIKELY(expr->kind != ExprBase::kString)) {
    pdp_warning("String access on non-string expression");
    return alternative;
  }
  const char *data = (const char *)expr + sizeof(ExprString);
  return StringSlice(data, expr->size);
}

int32_t ExprView::NumberOr(int32_t alternative) const {
  if (PDP_UNLIKELY(expr->kind != ExprBase::kString)) {
    pdp_warning("String access on non-string expression");
    return alternative;
  }
  const char *str = (const char *)expr + sizeof(ExprString);
  const bool negative = (*str == '-');
  str += negative;

  int32_t res = 0;
  for (size_t i = 0; i < expr->size; ++i) {
    if (PDP_UNLIKELY(str[i] < '0' || str[i] > '9')) {
      return alternative;
    }
    res *= 10;
    res += str[i] - '0';
  }
  if (PDP_UNLIKELY(negative)) {
    return -res;
  } else {
    return res;
  }
}

static void DebugFormatExpr(const ExprBase *expr, StringBuilder<> &builder) {
  if (expr->kind == ExprBase::kString) {
    StringSlice s((char *)expr + sizeof(ExprString), expr->size);
    builder.Append(s);
  } else if (expr->kind == ExprBase::kList) {
    if (expr->size == 0) {
      builder.Append("[]");
    } else {
      builder.Append('[');
      ExprBase **elements = reinterpret_cast<ExprBase **>((char *)expr + sizeof(ExprList));
      DebugFormatExpr(elements[0], builder);
      for (size_t i = 1; i < expr->size; ++i) {
        builder.Append(", ");
        DebugFormatExpr(elements[i], builder);
      }
      builder.Append(']');
    }
  } else if (expr->kind == ExprBase::kTuple) {
    if (expr->size == 0) {
      builder.Append("{}");
    } else {
      const ExprTuple *tuple = static_cast<const ExprTuple *>(expr);
      ExprTuple::Result *results = tuple->results;
      builder.AppendFormat("{{}=", StringSlice(results[0].key));
      DebugFormatExpr(results[0].value, builder);
      for (size_t i = 1; i < expr->size; ++i) {
        builder.AppendFormat(", {}=", StringSlice(results[i].key));
        DebugFormatExpr(results[i].value, builder);
      }
      builder.Append('}');
    }
  } else {
    pdp_assert(false);
  }
}

void ExprView::Print() {
  StringBuilder builder;
  DebugFormatExpr(expr, builder);
  pdp_info("NiceExpr={}", builder.GetSlice());
}

}  // namespace pdp

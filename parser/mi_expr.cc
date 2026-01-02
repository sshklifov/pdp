#include "mi_expr.h"

#include "core/log.h"
#include "external/ankerl_hash.h"

namespace pdp {

// static StringSlice ExprTypeToString(ExprBase::Kind kind) {
//   switch (kind) {
//     case ExprBase::kString:
//       return "c-string";
//     case ExprBase::kList:
//       return "list";
//     case ExprBase::kTuple:
//       return "tuple";
//     default:
//       pdp_assert(false);
//       return "???";
//   }
// }

uint32_t MiNiceExpr::Count() const {
  switch (expr->kind) {
    case MiExprBase::kTuple:
    case MiExprBase::kList:
      return expr->size;
    default:
      return 0;
  }
}

MiNiceExpr::MiNiceExpr(const MiExprBase *expr) : expr(expr) {}

MiNiceExpr MiNiceExpr::operator[](uint32_t index) {
  if (PDP_UNLIKELY(expr->kind != MiExprBase::kList)) {
    pdp_warning("List access on non-list expression");
    return nullptr;
  }

  if (PDP_UNLIKELY(index >= expr->size)) {
    pdp_warning("List access out of range");
    return nullptr;
  }
  MiExprBase **elements = reinterpret_cast<MiExprBase **>((char *)expr + sizeof(MiExprList));
  return elements[index];
}

MiNiceExpr MiNiceExpr::operator[](const StringSlice &key) {
  if (PDP_UNLIKELY(expr->kind != MiExprBase::kTuple)) {
    pdp_warning("Tuple access on non-tuple expression");
    return nullptr;
  }

  const MiExprTuple *tuple = static_cast<const MiExprTuple *>(expr);
  uint32_t num_elements = tuple->size;
  uint32_t hash = ankerl::unordered_dense::hash(key.Begin(), key.Size());
  for (uint32_t i = 0; i < num_elements; ++i) {
    if (PDP_UNLIKELY(tuple->hashes[i] == hash)) {
      MiExprTuple::Result *result = tuple->results + i;
      if (PDP_LIKELY(key == result->key)) {
        return result->value;
      }
    }
  }
  return nullptr;
}

StringSlice MiNiceExpr::StringOr(const StringSlice &alternative) const {
  if (PDP_UNLIKELY(expr->kind != MiExprBase::kString)) {
    pdp_warning("String access on non-string expression");
    return alternative;
  }
  const char *data = (const char *)expr + sizeof(MiExprString);
  return StringSlice(data, expr->size);
}

int32_t MiNiceExpr::NumberOr(int32_t alternative) const {
  if (PDP_UNLIKELY(expr->kind != MiExprBase::kString)) {
    pdp_warning("String access on non-string expression");
    return alternative;
  }
  const char *str = (const char *)expr + sizeof(MiExprString);
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

static void DebugFormatExpr(const MiExprBase *expr, StringBuilder<> &builder) {
  if (expr->kind == MiExprBase::kString) {
    StringSlice s((char *)expr + sizeof(MiExprString), expr->size);
    builder.Append(s);
  } else if (expr->kind == MiExprBase::kList) {
    if (expr->size == 0) {
      builder.Append("[]");
    } else {
      builder.Append('[');
      MiExprBase **elements = reinterpret_cast<MiExprBase **>((char *)expr + sizeof(MiExprList));
      DebugFormatExpr(elements[0], builder);
      for (size_t i = 1; i < expr->size; ++i) {
        builder.Append(", ");
        DebugFormatExpr(elements[i], builder);
      }
      builder.Append(']');
    }
  } else if (expr->kind == MiExprBase::kTuple) {
    if (expr->size == 0) {
      builder.Append("{}");
    } else {
      const MiExprTuple *tuple = static_cast<const MiExprTuple *>(expr);
      MiExprTuple::Result *results = tuple->results;
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

void MiNiceExpr::Print() {
  StringBuilder builder;
  DebugFormatExpr(expr, builder);
  pdp_info("NiceExpr={}", builder.GetSlice());
}

}  // namespace pdp

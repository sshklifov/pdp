#include "mi_parser.h"
#include "core/log.h"
#include "external/ankerl_hash.h"
#include "tracing/trace_likely.h"

namespace pdp {

bool IsMiIdentifier(char c) {
  return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_' || c == '-';
}

MiFirstPass::MiFirstPass(const StringSlice &s)
    : input(s), nesting_stack(50), sizes_stack(500), total_bytes(0) {}

bool MiFirstPass::ReportError(const StringSlice &msg) {
  auto context_len = input.Size() > 50 ? 50 : input.Size();
  pdp_error("{} at {}", msg, input.GetLeft(context_len));
  return false;
}

void MiFirstPass::PushSizeOnStack(uint32_t num_elements) {
  auto ptr = sizes_stack.NewElement();
  ptr->num_elements = num_elements;
  ptr->total_string_size = 0;
}

bool MiFirstPass::ParseResult() {
  auto it = input.Find('=');
  if (PDP_UNLIKELY(it == input.End())) {
    return ReportError("Expecting variable=...");
  }

  const uint32_t length_plus_one = it - input.Begin() + 1;
  sizes_stack[nesting_stack.Top()].total_string_size += length_plus_one;

  input.DropLeft(length_plus_one);
  return ParseValue();
}

bool MiFirstPass::ParseValue() {
  if (PDP_UNLIKELY(input.Empty())) {
    return ReportError("Expecting value but got empty string");
  }
  switch (input[0]) {
    case '"':
      return ParseString();
    case '[':
    case '{':
      return ParseListOrTuple();
    default:
      return ReportError("Expecting value but got invalid first char");
  }
}

bool MiFirstPass::ParseString() {
  pdp_assert(input.StartsWith('"'));

  auto it = input.Begin() + 1;
  uint32_t num_skipped = 2;
  while (it < input.End() && *it != '\"') {
    const bool skip_extra = (*it == '\\');
    num_skipped += skip_extra;
    it += (1 + skip_extra);
  }

  if (PDP_UNLIKELY(it == input.End())) {
    return ReportError("Unterminated c-string!");
  }

  const uint32_t length = it - input.Begin() + 1;
  PushSizeOnStack(length - num_skipped);
  total_bytes += AlignmentTraits::AlignUp(sizeof(ExprString) + length - num_skipped);

  input.DropLeft(length);
  return true;
}

bool MiFirstPass::ParseListOrTuple() {
  pdp_assert(input.StartsWith('[') || input.StartsWith('{'));
  input.DropLeft(1);

  nesting_stack.Push(sizes_stack.Size());
  PushSizeOnStack(0);
  // Note: Don't know yet if this is a list or tuple...
  return true;
}

bool MiFirstPass::ParseResultOrValue() {
  if (PDP_UNLIKELY(input.Empty())) {
    return ReportError("Expecting result or value but got nothing");
  }
  switch (input[0]) {
    case '"':
      return ParseString();
    case '[':
    case '{':
      return ParseListOrTuple();
    default:
      return ParseResult();
  }
}

void MiFirstPass::AccumulateBytes() {
  const auto &record = sizes_stack[nesting_stack.Top()];
  if (record.total_string_size > 0) {
    total_bytes += AlignmentTraits::AlignUp(sizeof(ExprTuple));
    total_bytes += AlignmentTraits::AlignUp(record.num_elements * sizeof(uint32_t));
    total_bytes += AlignmentTraits::AlignUp(record.num_elements * sizeof(ExprTuple::Result));
    total_bytes += AlignmentTraits::AlignUp(record.total_string_size);
  } else {
    total_bytes += AlignmentTraits::AlignUp(sizeof(ExprList));
    total_bytes += AlignmentTraits::AlignUp(record.num_elements * sizeof(ExprBase *));
  }
}

bool MiFirstPass::Parse() {
  if (PDP_UNLIKELY(input.Empty())) {
    total_bytes = AlignmentTraits::AlignUp(sizeof(ExprTuple));
    return true;
  }

  // Record first result speculatively (for correct order).
  nesting_stack.Push(sizes_stack.Size());
  PushSizeOnStack(1);

  bool okay = ParseResultOrValue();
  while (okay && !input.Empty()) {
    if (PDP_UNLIKELY(nesting_stack.Empty())) {
      return ReportError("No open list/tuple in scope");
    }
    switch (input[0]) {
      case ']':
      case '}':
        AccumulateBytes();
        input.DropLeft(1);
        nesting_stack.Pop();
        break;

      case ',':
        input.DropLeft(1);
        [[fallthrough]];
      default:
        sizes_stack[nesting_stack.Top()].num_elements += 1;
        okay = ParseResultOrValue();
    }
  }
  if (PDP_LIKELY(okay && nesting_stack.Size() == 1)) {
    AccumulateBytes();
    return true;
  } else if (nesting_stack.Size() > 1) {
    ReportError("Unexpected end of input: unclosed list or tuple");
  } else if (nesting_stack.Size() == 0) {
    ReportError("Syntax error, extra closing bracket");
  }
  nesting_stack.Destroy();
  sizes_stack.Destroy();
  return false;
}

MISecondPass::MISecondPass(const StringSlice &s, MiFirstPass &first_pass)
    : input(s),
      first_pass_stack(std::move(first_pass.sizes_stack)),
      first_pass_marker(0),
      arena(first_pass.total_bytes),
      second_pass_stack(50) {}

ExprBase *MISecondPass::ReportError(const StringSlice &msg) {
  auto context_len = input.Size() > 50 ? 50 : input.Size();
  pdp_error("{} at {}", msg, input.GetLeft(context_len));
  return nullptr;
}

ExprBase *MISecondPass::ParseResult() {
  pdp_assert(!input.Empty());

  pdp_assert(!second_pass_stack.Empty());
  char *__restrict string_table_ptr = second_pass_stack.Top().string_table_ptr;
  pdp_assert(string_table_ptr);
  second_pass_stack.Top().tuple_members->key = string_table_ptr;

  const char *__restrict it = input.Begin();
  while (IsMiIdentifier(*it)) {
    pdp_assert(it < input.End());
    *string_table_ptr = *it;
    ++string_table_ptr;
    ++it;
  }
  const bool failed = (*it != '=');
  if (PDP_UNLIKELY(failed)) {
    return ReportError("Expecting variable=...");
  }

  *string_table_ptr = '\0';
  second_pass_stack.Top().string_table_ptr = string_table_ptr + 1;

  const size_t length = it - input.Begin();
  const uint32_t hash = ankerl::unordered_dense::hash(input.Begin(), length);
  *second_pass_stack.Top().hash_table_ptr = hash;
  ++second_pass_stack.Top().hash_table_ptr;

  input.DropLeft(length + 1);
  return ParseValue();
}

ExprBase *MISecondPass::ParseValue() {
  if (PDP_UNLIKELY(input.Empty())) {
    return ReportError("Expecting value but got empty string");
  }
  switch (input[0]) {
    case '"':
      return ParseString();
    case '[':
    case '{':
      return ParseListOrTuple();
    default:
      return ReportError("Expecting value but got invalid first char");
  }
}

ExprBase *MISecondPass::ParseString() {
  uint32_t length = first_pass_stack[first_pass_marker].num_elements;
  pdp_assert(first_pass_stack[first_pass_marker].total_string_size == 0);
  ++first_pass_marker;

  ExprString *expr = static_cast<ExprString *>(arena.Allocate(sizeof(ExprString) + length));
  expr->kind = ExprBase::kString;
  expr->size = length;
  char *__restrict payload = expr->payload;

  const char *__restrict it = input.Begin();
  pdp_assert(it < input.End() && *it == '"');
  ++it;

  while (*it != '\"') {
    pdp_assert(it < input.End());
    if (PDP_TRACE_LIKELY(*it != '\\')) {
      *payload = *it;
      ++payload;
      ++it;
    } else {
      pdp_assert(it + 1 < input.End());
      switch (it[1]) {
        case 'n':
        case 'r':
          *payload = '\n';
          break;
        case 't':
          *payload = ' ';
          break;
        case '\\':
          *payload = '\\';
          break;
        case '"':
        default:
          *payload = it[1];
      }
      ++payload;
      it += 2;
    }
  }

  pdp_assert(it < input.End() && *it == '\"');
  pdp_assert(payload - expr->payload == expr->size);

  input.DropLeft(it + 1);
  return expr;
}

ExprBase *MISecondPass::CreateListOrTuple() {
  uint32_t size = first_pass_stack[first_pass_marker].num_elements;
  uint32_t string_table_size = first_pass_stack[first_pass_marker].total_string_size;
  const bool is_tuple = string_table_size > 0;
  ++first_pass_marker;

  if (is_tuple) {
    ExprTuple *tuple = static_cast<ExprTuple *>(arena.AllocateUnchecked(sizeof(ExprTuple)));
    tuple->kind = ExprBase::kTuple;
    tuple->size = size;
    tuple->hashes = static_cast<uint32_t *>(arena.AllocateOrNull(size * sizeof(uint32_t)));
    tuple->results =
        static_cast<ExprTuple::Result *>(arena.AllocateOrNull(size * sizeof(ExprTuple::Result)));
    char *string_table = static_cast<char *>(arena.AllocateOrNull(string_table_size));

    auto *expr_record = second_pass_stack.NewElement();
    expr_record->expr = tuple;
    expr_record->tuple_members = tuple->results;
    expr_record->hash_table_ptr = tuple->hashes;
    expr_record->string_table_ptr = string_table;
#ifdef PDP_ENABLE_ASSERT
    expr_record->record_end = string_table + string_table_size;
#endif

    // Locality checks
    pdp_assert((char *)tuple->hashes - (char *)tuple == sizeof(ExprTuple));
    pdp_assert((char *)tuple->results - (char *)tuple->hashes <=
               static_cast<ptrdiff_t>((size + 1) * sizeof(uint32_t)));
    pdp_assert(string_table - (char *)tuple->results ==
               static_cast<ptrdiff_t>(size * sizeof(ExprTuple::Result)));
    return tuple;
  } else {
    ExprList *list = static_cast<ExprList *>(arena.AllocateUnchecked(sizeof(ExprList)));
    list->kind = ExprBase::kList;
    list->size = size;
    ExprBase **members = static_cast<ExprBase **>(arena.AllocateOrNull(size * sizeof(ExprBase *)));

    auto *expr_record = second_pass_stack.NewElement();
    expr_record->expr = list;
    expr_record->list_members = members;
    expr_record->string_table_ptr = nullptr;
#ifdef PDP_ENABLE_ASSERT
    expr_record->record_end = (char *)members + size * sizeof(ExprBase *);
#endif

    // Validity allocation check
    pdp_assert(!members || (char *)members - (char *)list == sizeof(ExprList));
    return list;
  }
}

ExprBase *MISecondPass::ParseListOrTuple() {
  pdp_assert(input.StartsWith('[') || input.StartsWith('{'));
  input.DropLeft(1);

  return CreateListOrTuple();
}

ExprBase *MISecondPass::ParseResultOrValue() {
  auto parent_pos = second_pass_stack.Size() - 1;

  ExprBase *expr = nullptr;
  pdp_assert(!input.Empty());
  switch (input[0]) {
    case '"':
      expr = ParseString();
      break;
    case '[':
    case '{':
      expr = ParseListOrTuple();
      break;
    default:
      expr = ParseResult();
  }

  if (PDP_LIKELY(expr)) {
    const bool is_tuple = second_pass_stack[parent_pos].string_table_ptr;
    if (is_tuple) {
      second_pass_stack[parent_pos].tuple_members->value = expr;
      ++second_pass_stack[parent_pos].tuple_members;
    } else {
      *second_pass_stack[parent_pos].list_members = expr;
      ++second_pass_stack[parent_pos].list_members;
    }
  }
  return expr;
}

ExprBase *MISecondPass::Parse() {
  if (PDP_UNLIKELY(input.Empty())) {
    ExprTuple *tuple = static_cast<ExprTuple *>(arena.AllocateUnchecked(sizeof(ExprTuple)));
    tuple->kind = ExprBase::kTuple;
    tuple->size = 0;
    tuple->hashes = nullptr;
    tuple->results = nullptr;
    return tuple;
  }

  ExprBase *root = CreateListOrTuple();
  bool okay = (root != nullptr);
  while (okay && !input.Empty()) {
    switch (input[0]) {
      case ']':
      case '}':
        if (second_pass_stack.Top().string_table_ptr) {
          pdp_assert(second_pass_stack.Top().string_table_ptr <=
                     second_pass_stack.Top().record_end);
          [[maybe_unused]]
          ExprTuple *tuple = static_cast<ExprTuple *>(second_pass_stack.Top().expr);
          pdp_assert(second_pass_stack.Top().tuple_members - tuple->results == tuple->size);
          pdp_assert(second_pass_stack.Top().hash_table_ptr - tuple->hashes == tuple->size);
        } else {
          pdp_assert(second_pass_stack.Top().list_members <= second_pass_stack.Top().record_end);
        }
        second_pass_stack.Pop();
        input.DropLeft(1);
        break;

      case ',':
        input.DropLeft(1);
        [[fallthrough]];
      default:
        okay = ParseResultOrValue();
    }
  }
  pdp_assert(first_pass_marker == first_pass_stack.Size());
  pdp_assert(second_pass_stack.Size() == 1);
  if (PDP_LIKELY(okay)) {
    return root;
  }
  return nullptr;
}

}  // namespace pdp

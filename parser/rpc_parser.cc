#include "rpc_parser.h"

#include "core/log.h"
#include "external/ankerl_hash.h"
#include "tracing/trace_likely.h"

namespace pdp {



#if 0
RpcFirstPass::RpcFirstPass(const StringSlice &s)
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
  total_bytes += ArenaTraits::AlignUp(sizeof(MiExprString) + length - num_skipped);

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
    total_bytes += ArenaTraits::AlignUp(sizeof(MiExprTuple));
    total_bytes += ArenaTraits::AlignUp(record.num_elements * sizeof(uint32_t));
    total_bytes += ArenaTraits::AlignUp(record.num_elements * sizeof(MiExprTuple::Result));
    total_bytes += ArenaTraits::AlignUp(record.total_string_size);
  } else {
    total_bytes += ArenaTraits::AlignUp(sizeof(MiExprList));
    total_bytes += ArenaTraits::AlignUp(record.num_elements * sizeof(MiExprBase *));
  }
}

bool MiFirstPass::Parse() {
  if (PDP_UNLIKELY(input.Empty())) {
    total_bytes = ArenaTraits::AlignUp(sizeof(MiExprTuple));
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
        // follow-through
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
#endif

}  // namespace pdp

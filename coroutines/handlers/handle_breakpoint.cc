#include "coroutines/handlers.h"

namespace pdp {

void ClearBreakpointSign(DebugCoordinator &d, const StringSlice &in_id, bool should_delete) {
  for (auto &[id, br] : d.Breakpoints().GetAliases(in_id)) {
    if (!br.fullname.Empty() && br.extmark > 0) {
      d.VimDriver().DeleteBreakpointMark(br.fullname.ToSlice(), br.extmark);
      br.extmark = 0;
    }
  }
  // Clear logically if breakpoint was deleted
  if (should_delete) {
    d.Breakpoints().Delete(in_id);
  }
}

#if 0
void PlaceBreakpointSign(DebugCoordinator &d, const StringSlice &id) {
  auto it = d.Breakpoints().Find(id);
  pdp_assert(it != d.Breakpoints().End());

  const bool check_file = !it->value.fullname.Empty() && FileReadable(it->value.fullname.Cstr());
  if (PDP_UNLIKELY(!check_file)) {
    return;
  }
  const bool placed = (it->value.extmark > 0);
  if (PDP_UNLIKELY(placed)) {
    return;
  }
  // TODO
  // awaiter = d.VimDriver().PromiseBufferNumber(it->value.fullname.ToSlice());

  auto bufnr = co_await awaiter;
  if (PDP_LIKELY(bufnr < 0)) {
    co_return;
  }

  PDP_BLOCK() {
    auto it = d.Breakpoints().Find(id.ToSlice());
    const bool deleted_during_suspend = it == d.Breakpoints().End();
    if (PDP_UNLIKELY(deleted_during_suspend)) {
      co_return;
    }
    awaiter =
        d.VimDriver().PromiseBreakpointMark(id.ToSlice(), bufnr, it->value.lnum, it->value.enabled);
  }

  auto extmark = co_await awaiter;
  auto it = d.Breakpoints().Find(id.ToSlice());
  if (PDP_LIKELY(it != d.Breakpoints().End())) {
    it->value.extmark = extmark;
  }
}
#endif

void FormatBreakpointMessage(DebugCoordinator &d, GdbExprView bkpt,
                             BreakpointTable::NoSuspendIterator it) {
  const Breakpoint &br = it->value;
  bool jumpable = false;

  MessageBuilder builder;
  builder.AppendFormat("debugIdentifier", "*{}", it->key.ToSlice());
  if (br.type & Breakpoint::kWatchBit) {
    builder.Append(" when ", "Normal");
    builder.AppendFormat("Bold", "\"{}\"", bkpt["what"].RequireStr());
    switch (br.type & (Breakpoint::kWatchReadBit | Breakpoint::kWatchWriteBit)) {
      case Breakpoint::kWatchReadBit:
        builder.Append(" is read ", "Normal");
        break;
      case Breakpoint::kWatchWriteBit:
        builder.Append(" is written ", "Normal");
        break;
      default:
        builder.Append(" is accessed ", "Normal");
        break;
    }
  } else if (br.type == Breakpoint::kCatch) {
    builder.AppendFormat("Bold", "\"{}\"", bkpt["what"].RequireStr());
  } else if (br.type == Breakpoint::kBreak) {
    jumpable = !br.fullname.Empty() && FileReadable(br.fullname.Cstr());
    builder.Append(" in ", "Normal");
    auto hl = jumpable && br.enabled ? "debugJumpable" : "debugLocation";
    auto location = bkpt["at"];
    if (location) {
      builder.Append(location.RequireStr(), hl);
    } else {
      location = bkpt["func"];
      if (location) {
        builder.Append(location.RequireStr(), hl);
      } else if (jumpable) {
        StringSlice basename = GetBasename(br.fullname.Cstr());
        builder.AppendFormat(hl, "{}:{}", basename, br.lnum);
      } else {
        StringSlice addr = bkpt["addr"].StrOr("???");
        builder.Append(addr, hl);
      }
    }
  }
  const auto message_length = builder.GetJoinedMessageLength();
  d.VimDriver().ShowMessage(builder);
  if (!br.enabled) {
    d.VimDriver().HighlightLastLine(0, message_length, "@markup.strikethrough");
  }
  if (jumpable) {
    d.InsertJump(br.fullname.ToSlice(), br.lnum);
  }
}

Coroutine HandleNewBreakpoint(DebugCoordinator &d, UniquePtr<ExprBase> expr) {
  GdbExprView dict(expr);

  auto bkpt = dict["bkpt"];
  if (bkpt["type"].RequireStr() != "breakpoint") {
    // Display a message to the user
    if (bkpt["type"].RequireStr() == "catchpoint") {
      d.VimDriver().ShowNormal("Catchpoint {} ({})", bkpt["number"].RequireStr(),
                               bkpt["what"].RequireStr());
    } else if (bkpt["type"].RequireStr().MemMem("watchpoint")) {
      d.VimDriver().ShowNormal("Watchpoint {} ({})", bkpt["number"].RequireStr(),
                               bkpt["what"].RequireStr());
    }
    co_return;
  }

  if (bkpt["pending"]) {
    d.VimDriver().ShowNormal("Breakpoint {} ({}) pending", bkpt["number"].RequireStr(),
                             bkpt["pending"].RequireStr());
    co_return;
  }

  ClearBreakpointSign(d, bkpt["number"].RequireStr(), false);

  const bool check_race_condition = d.GetInferiorPid() > 0;

  auto addr = bkpt["addr"];
  if (addr && addr == "<MULTIPLE>") {
    auto locations = bkpt["locations"];
    for (size_t i = 0; i < locations.Count(); ++i) {
      auto [it, is_new] = d.Breakpoints().Insert(locations[i], bkpt);
      if (it->value.type == Breakpoint::kBreak) {
        PlaceBreakpointSign(d, it->key.Copy());
      }
      if (is_new && check_race_condition) {
        FormatBreakpointMessage(d, bkpt, it);
      }
    }
  } else {
    auto [it, is_new] = d.Breakpoints().Insert(bkpt, nullptr);
    if (it->value.type == Breakpoint::kBreak) {
      PlaceBreakpointSign(d, it->key.Copy());
    }
    if (is_new && check_race_condition) {
      FormatBreakpointMessage(d, bkpt, it);
    }
  }
}

}  // namespace pdp

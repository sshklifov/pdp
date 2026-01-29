#include "breakpoint_table.h"
#include "system/file_descriptor.h"

#include <linux/limits.h>
#include <unistd.h>

namespace pdp {

Breakpoint::Breakpoint() : type(kUnknown), enabled(0), lnum(-1), extmark(0) {}

BreakpointTable::BreakpointTable() : buffer(PATH_MAX + 1) {}

FixedString BreakpointTable::RealPathFromSlice(const StringSlice &str) {
  auto len = str.Size() < PATH_MAX ? str.Size() : PATH_MAX;
  memcpy(buffer.Get(), str.Data(), len);
  buffer[len] = '\0';
  return RealPath(buffer.Get());
}

BreakpointTable::InsertionResult BreakpointTable::Insert(GdbExprView bkpt, GdbExprView parent) {
  StringSlice id = bkpt["number"].RequireStr();
  auto [it, inserted] = table.Emplace(id);
  Breakpoint *new_br = &it->value;

  new_br->enabled = bkpt["enabled"].RequireInt();
  auto fullname = bkpt["fullname"];
  if (fullname) {
    new_br->lnum = bkpt["line"].RequireInt();
    new_br->fullname.Reset(RealPathFromSlice(fullname.RequireStr()));
  }

  auto type = bkpt["type"];
  if (PDP_LIKELY(type)) {
    auto type_str = type.RequireStr();
    if (PDP_LIKELY(type_str == "breakpoint")) {
      new_br->type = Breakpoint::kBreak;
    } else if (PDP_LIKELY(type_str.EndsWith("watchpoint"))) {
      if (type_str.StartsWith("acc")) {
        new_br->type = Breakpoint::kWatchAcc;
      } else if (type_str.StartsWith("read")) {
        new_br->type = Breakpoint::kWatchRead;
      } else {
        new_br->type = Breakpoint::kWatch;
      }
    } else if (type_str == "catchpoint") {
      new_br->type = Breakpoint::kCatch;
    } else {
      new_br->type = Breakpoint::kUnknown;
    }
  } else {
    new_br->type = Breakpoint::kUnknown;
  }

  if (parent) {
    StringSlice parent_id = parent["id"].RequireStr();
    new_br->enabled = new_br->enabled && (parent["enabled"] == "y");

    auto [aliases_it, _] = aliases.Emplace(parent_id);
    aliases_it->value.MemCopy(id.Data(), id.Size());
    aliases_it->value += '\0';
  }
  return {it, inserted};
}

void BreakpointTable::Delete(const StringSlice &id) {
  auto aliases_it = aliases.Find(id);
  if (aliases_it != aliases.End()) {
    for (auto alias : aliases_it->value.SplitByNull()) {
      auto table_it = table.Find(alias);
      pdp_assert(table_it != table.End());
      table.Erase(table_it);
    }
    aliases.Erase(aliases_it);
  } else {
    auto it = table.Find(id);
    if (it != table.End()) {
      table.Erase(it);
    }
  }
}

BreakpointAliases BreakpointTable::GetAliases(const StringSlice &id) {
  auto aliases_it = aliases.Find(id);
  if (aliases_it != aliases.End()) {
    return BreakpointAliases(&table, aliases_it->value);
  } else {
    auto table_it = table.Find(id);
    if (table_it != table.End()) {
      return BreakpointAliases(&table, table_it->key);
    } else {
      return BreakpointAliases(&table);
    }
  }
}

BreakpointTable::NoSuspendIterator BreakpointTable::Find(const StringSlice &id) {
  return NoSuspendIterator(table.Find(id));
}

BreakpointTable::NoSuspendIterator BreakpointTable::End() { return NoSuspendIterator(table.End()); }

}  // namespace pdp

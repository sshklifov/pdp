#pragma once

#include "external/emhash8.h"
#include "parser/expr.h"
#include "strings/fixed_string.h"
#include "system/no_suspend_lock.h"

namespace pdp {

struct Breakpoint {
  enum Type {
    kUnknown = 0,
    kWatchBit = 4,
    kWatchReadBit = 1,
    kWatchWriteBit = 2,
    kWatch = kWatchBit | kWatchWriteBit,
    kWatchRead = kWatchBit | kWatchReadBit,
    kWatchAcc = kWatchBit | kWatchRead | kWatchWriteBit,
    kBreak = 8,
    kCatch = 16,
  };

  Breakpoint();

  FixedString fullname;
  FixedString script;
  Type type;
  bool enabled;
  int lnum;
  int extmark;
};

template <>
struct CanReallocate<Breakpoint> : std::true_type {};

struct BreakpointAliases {
  using _Map = emhash8::StringMap<Breakpoint>;

  struct NoSuspendIterator {
    NoSuspendIterator(_Map *t) : table(t), alias_it(NullTerminateSplit::Iterator::End()) {}

    NoSuspendIterator(_Map *t, NullTerminateSplit::Iterator it) : table(t), alias_it(it) {}

    static NoSuspendIterator End(_Map *t) { return NoSuspendIterator(t); }

    _Map::Entry &operator*() {
      auto it = table->Find(*alias_it);
      pdp_assert(it != table->End());
      return *it;
    }

    NoSuspendIterator &operator++() {
      ++alias_it;
      return (*this);
    }

    bool operator==(const NoSuspendIterator &rhs) const {
      pdp_assert(table == rhs.table);
      return alias_it == rhs.alias_it;
    }

    bool operator!=(const NoSuspendIterator &rhs) const {
      pdp_assert(table == rhs.table);
      return alias_it != rhs.alias_it;
    }

   private:
    _Map *table;
    NullTerminateSplit::Iterator alias_it;
    NoSuspendGuard suspend_guard;
  };

  BreakpointAliases(_Map *t, StringVector &v) : fwd_table(t), fwd_it(v.Begin(), v.End()) {}

  BreakpointAliases(_Map *t, FixedString &f) : fwd_table(t), fwd_it(f.Begin(), f.End()) {}

  BreakpointAliases(_Map *t) : fwd_table(t), fwd_it(NullTerminateSplit::Iterator::End()) {}

  NoSuspendIterator begin() { return NoSuspendIterator(fwd_table, fwd_it); }

  NoSuspendIterator end() { return NoSuspendIterator::End(fwd_table); }

 private:
  _Map *fwd_table;
  NullTerminateSplit::Iterator fwd_it;
};

struct BreakpointTable : public NonCopyableNonMovable {
  using _Entry = emhash8::StringMap<Breakpoint>::Entry;

  struct NoSuspendIterator : public NonMoveable {
    NoSuspendIterator(_Entry *it) : it(it) {}

    NoSuspendIterator(const NoSuspendIterator &rhs) : it(rhs.it) {}

    NoSuspendIterator &operator=(const NoSuspendIterator &rhs) = delete;

    _Entry &operator*() { return *it; }

    _Entry *operator->() { return &(**this); }

    NoSuspendIterator &operator++() {
      ++it;
      return (*this);
    }

    bool operator==(const NoSuspendIterator &rhs) const { return it == rhs.it; }

    bool operator!=(const NoSuspendIterator &rhs) const { return it != rhs.it; }

   private:
    _Entry *it;
    NoSuspendGuard suspend_guard;
  };

  struct InsertionResult {
    NoSuspendIterator it;
    bool is_new;
  };

  BreakpointTable();

  InsertionResult Insert(GdbExprView bkpt, GdbExprView parent);

  void Delete(const StringSlice &id);

  BreakpointAliases GetAliases(const StringSlice &id);

  NoSuspendIterator Find(const StringSlice &id);

  NoSuspendIterator End();

 private:
  FixedString RealPathFromSlice(const StringSlice &str);

  emhash8::StringMap<Breakpoint> table;
  emhash8::StringMap<StringVector> aliases;
  StringBuffer buffer;
};

}  // namespace pdp

#pragma once

#include "data/no_suspend_lock.h"
#include "external/emhash8.h"
#include "parser/expr.h"
#include "strings/fixed_string.h"

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
};

template <>
struct CanReallocate<Breakpoint> : std::true_type {};

// TODO not super happy, overengineered.
struct BreakpointAliases {
  using _Map = emhash8::StringMap<Breakpoint>;

  struct Iterator {
    Iterator(_Map *t, NullTerminateSplit::Iterator it) : table(t), alias_it(it) {}

    static Iterator End(_Map *t) { return Iterator(t); }

    _Map::Entry &operator*() {
      auto it = table->Find(*alias_it);
      pdp_assert(it != table->End());
      return *it;
    }

    Iterator &operator++() {
      ++alias_it;
      return (*this);
    }

    bool operator==(const Iterator &rhs) const {
      pdp_assert(table == rhs.table);
      return alias_it == rhs.alias_it;
    }

    bool operator!=(const Iterator &rhs) const {
      pdp_assert(table == rhs.table);
      return alias_it != rhs.alias_it;
    }

   private:
    Iterator(_Map *t) : table(t), alias_it(NullTerminateSplit::Iterator::End()) {}

    _Map *table;
    NullTerminateSplit::Iterator alias_it;
    // NoSuspendGuard suspend_guard;
  };

  BreakpointAliases(_Map *t, StringVector &v) : fwd_table(t), fwd_it(v.Begin(), v.End()) {}

  BreakpointAliases(_Map *t, FixedString &f) : fwd_table(t), fwd_it(f.Begin(), f.End()) {}

  BreakpointAliases(_Map *t) : fwd_table(t), fwd_it(NullTerminateSplit::Iterator::End()) {}

  Iterator begin() { return Iterator(fwd_table, fwd_it); }

  Iterator end() { return Iterator::End(fwd_table); }

 private:
  _Map *fwd_table;
  NullTerminateSplit::Iterator fwd_it;
};

// TODO: unnecessary complicated, no need for indirection / additional layer of whatever this is.
struct BreakpointTable : public NonCopyableNonMovable {
  using Iterator = emhash8::StringMap<Breakpoint>::Entry *;
  using InsertResult = emhash8::StringMap<Breakpoint>::EmplaceResult;

  BreakpointTable();

  InsertResult Insert(GdbExprView bkpt, GdbExprView parent);

  void Delete(const StringSlice &id);

  BreakpointAliases GetAliases(const StringSlice &id);

  Iterator Find(const StringSlice &id);

  Iterator End();

 private:
  FixedString RealPathFromSlice(const StringSlice &str);

  emhash8::StringMap<Breakpoint> table;
  emhash8::StringMap<StringVector> aliases;
  StringBuffer buffer;
};

}  // namespace pdp

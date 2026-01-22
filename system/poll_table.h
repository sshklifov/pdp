#pragma once

#include "data/allocator.h"
#include "time_units.h"

#include <sys/poll.h>

namespace pdp {

struct PollTable {
  PollTable() {
    table = Allocate<struct pollfd>(allocator, max_size);
    size = 0;
  }

  ~PollTable() { Deallocate<struct pollfd>(allocator, table); }

  void Register(int fd, int events = POLLIN) {
    if (PDP_UNLIKELY(size >= max_size)) {
      PDP_UNREACHABLE("PollTable: overflow");
    }

    table[size].fd = fd;
    table[size].events = events;
    table[size].revents = 0;
    ++size;
  }

  bool Poll(Milliseconds timeout) {
    InsertionSort();

    pdp_assert(size > 0);
    int ret = poll(table, size, timeout.Get());
    if (ret <= 0) {
      Check(ret, "poll");
      return false;
    }
    return ret > 0;
  }

  int GetEventsUnchecked(int fd) const {
    auto idx = BinarySearch(fd);
    pdp_assert(table[idx].fd == fd);
    return table[idx].revents;
  }

  int GetEventsOrZero(int fd) const {
    auto idx = BinarySearch(fd);
    if (PDP_LIKELY(table[idx].fd == fd)) {
      return table[idx].revents;
    } else {
      return 0;
    }
  }

  bool HasInputEventsUnchecked(int fd) const { return GetEventsUnchecked(fd) & POLLIN; }

  bool HasInputEvents(int fd) const { return GetEventsOrZero(fd) & POLLIN; }

  void Reset() { size = 0; }

 private:
  void InsertionSort() {
    for (size_t i = 1; i < size; ++i) {
      pollfd curr = table[i];
      size_t j = i;
      while (j > 0 && table[j - 1].fd > curr.fd) {
        table[j] = table[j - 1];
        --j;
      }
      table[j] = curr;
    }
#ifdef PDP_ENABLE_ASSERT
    bool has_duplicates = false;
    for (size_t i = 1; i < size; ++i) {
      has_duplicates |= (table[i].fd == table[i - 1].fd);
    }
    pdp_assert(!has_duplicates);
#endif
  }

  size_t BinarySearch(int fd) const {
    size_t lo = 0, hi = size;
    while (hi - lo > 1) {
      size_t mid = (lo + hi) / 2;
      if (fd < table[mid].fd) {
        hi = mid;
      } else {
        lo = mid;
      }
    }
    return lo;
  }

  static constexpr size_t max_size = 32;

  struct pollfd *table;
  size_t size;

  DefaultAllocator allocator;
};

}  // namespace pdp

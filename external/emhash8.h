// emhash8::HashMap for C++14/17
// version 1.6.5
// https://github.com/ktprime/emhash/blob/master/hash_table8.hpp
//
// Licensed under the MIT License <http://opensource.org/licenses/MIT>.
// SPDX-License-Identifier: MIT
// Copyright (c) 2021-2023 Huang Yuanbing & bailuzhou AT 163.com
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE

#pragma once

#include "ankerl_hash.h"

#include "core/internals.h"
#include "data/allocator.h"

#include <cstdint>
#include <utility>

#define EMH_EMPTY(n) (0 > (int)(_index[n].next))
#define EMH_EQHASH(n, key_hash) (((uint32_t)(key_hash) & ~_mask) == (_index[n].slot & ~_mask))
#define EMH_NEW(key, args, bucket, key_hash)      \
  new (_pairs + _num_filled) Entry(key, args...); \
  _etail = bucket;                                \
  _index[bucket] = {bucket, _num_filled++ | ((uint32_t)(key_hash) & ~_mask)}

#define EMH_INDEX_INACTIVE (~0u)
#define EMH_INDEX_EAD 2

namespace pdp {

template <typename T>
struct Hash;

}  // namespace pdp

namespace emhash8 {

template <typename K, typename V, typename Alloc = pdp::DefaultAllocator>
class Map {
  struct Index {
    uint32_t next;
    uint32_t slot;
  };

  struct Entry {
    template <typename Arg, typename... Args>
    explicit Entry(Arg &&key, Args &&...args)
        : key(std::forward<Arg>(key)), value(std::forward<Args>(args)...) {}

    K key;
    V value;
  };

  static_assert(std::is_nothrow_destructible_v<Entry>, "K and V must be noexcept destructible");
  static_assert(std::is_trivially_move_constructible_v<Entry>,
                "K and V must be movable with realloc");
  static_assert(std::is_invocable_v<const pdp::Hash<K>, K>, "Hash function missing for K");

 public:
  constexpr static float EMH_DEFAULT_LOAD_FACTOR = 0.80f;
  constexpr static float EMH_MIN_LOAD_FACTOR = 0.25f;

  Map(Alloc alloc = Alloc(), uint32_t bucket = 2, float mlf = EMH_DEFAULT_LOAD_FACTOR) noexcept
      : allocator(alloc) {
#ifndef PDP_ENABLE_EXTERNAL_CODE
    pdp_error("Dynamic usage of emhas8::Map detected!");
    std::terminate();
#endif
    Init(bucket, mlf);
  }

  Map(const Map &rhs) = delete;

  Map(Map &&rhs) noexcept {
    Init(0);
    *this = std::move(rhs);
  }

  ~Map() noexcept {
    Clearkv();
    pdp::Deallocate<Entry>(allocator, _pairs);
    pdp::Deallocate<Index>(allocator, _index);
  }

  void operator=(const Map &rhs);

  Map &operator=(Map &&rhs) noexcept {
    if (this != &rhs) {
      Swap(rhs);
      rhs.Clear();
    }
    return *this;
  }

  void Swap(Map &rhs) {
    std::swap(_pairs, rhs._pairs);
    std::swap(_index, rhs._index);
    std::swap(_num_buckets, rhs._num_buckets);
    std::swap(_num_filled, rhs._num_filled);
    std::swap(_mask, rhs._mask);
    std::swap(_mlf, rhs._mlf);
    std::swap(_last, rhs._last);
    std::swap(_etail, rhs._etail);
  }

  Entry *Begin() { return _pairs; }
  Entry *End() { return _pairs + _num_filled; }

  const Entry *Begin() const { return _pairs; }
  const Entry *End() const { return _pairs + _num_filled; }

  uint32_t Size() const { return _num_filled; }
  bool Empty() const { return _num_filled == 0; }

  Entry *Find(const K &key) {
    uint32_t idx = FindFilledSlot(key);
    return _pairs + idx;
  }

  template <typename... Types>
  void EmplaceUnique(const K &key, Types &&...args) {
    CheckExpandNeed();
    const uint64_t key_hash = hasher(key);
    uint32_t bucket = FindUniqueBucket(key_hash);
    EMH_NEW(key, std::forward<Types>(args), bucket, key_hash);
  }

  template <typename... Types>
  Entry *Emplace(const K &key, Types &&...args) {
    CheckExpandNeed();

    const uint64_t key_hash = hasher(key);
    const uint32_t bucket = FindOrAllocate(key, key_hash);
    const bool bempty = EMH_EMPTY(bucket);
    if (bempty) {
      EMH_NEW(key, std::forward<Types>(args), bucket, key_hash);
    }

    const uint32_t slot = _index[bucket].slot & _mask;
    return _pairs + slot;
  }

  /// Erase an element from the hash table.
  bool Erase(const K &key) {
    const uint64_t key_hash = hasher(key);
    const uint32_t sbucket = FindFilledBucket(key, key_hash);
    if (sbucket == EMH_INDEX_INACTIVE) {
      return 0;
    }

    const uint32_t main_bucket = key_hash & _mask;
    EraseSlot(sbucket, main_bucket);
    return 1;
  }

  void Erase(const Entry *it) {
    const uint32_t slot = it - _pairs;
    uint32_t main_bucket;
    const uint32_t sbucket = FindSlotBucket(slot, main_bucket);
    EraseSlot(sbucket, main_bucket);
  }

  /// Remove all elements, keeping full capacity.
  void Clear() {
    Clearkv();

    if (_num_filled > 0) {
      memset((char *)_index, EMH_INDEX_INACTIVE, sizeof(_index[0]) * _num_buckets);
    }

    _last = _num_filled = 0;
    _etail = EMH_INDEX_INACTIVE;
  }

 private:
  /// Returns average number of elements per bucket.
  inline float LoadFactor() const { return static_cast<float>(_num_filled) / (_mask + 1); }

  void MaxLoadFactor(float mlf) {
    if (mlf < 0.992 && mlf > EMH_MIN_LOAD_FACTOR) {
      _mlf = (uint32_t)((1 << 27) / mlf);
      if (_num_buckets > 0) {
        Rehash(_num_buckets);
      }
    }
  }

  inline constexpr float MaxLoadFactor() const { return (1 << 27) / (float)_mlf; }
  inline constexpr uint32_t MaxBucketCount() const { return (1ull << (sizeof(uint32_t) * 8 - 1)); }

  void Init(uint32_t bucket, float mlf = EMH_DEFAULT_LOAD_FACTOR) {
    _pairs = nullptr;
    _index = nullptr;
    _mask = _num_buckets = 0;
    _num_filled = 0;
    _mlf = (uint32_t)((1 << 27) / EMH_DEFAULT_LOAD_FACTOR);
    MaxLoadFactor(mlf);
    Rehash(bucket);
  }

  void Clearkv() {
    if (!std::is_trivially_destructible_v<Entry>) {
      pdp_trace_once("Found non trivially destructible object in {}", __PRETTY_FUNCTION__);
      while (_num_filled--) _pairs[_num_filled].~Entry();
    }
  }

  void ReallocBucket(uint32_t num_buckets) {
    _pairs = pdp::Reallocate<Entry>(allocator, _pairs, num_buckets);
    pdp_assert(_pairs);
  }

  void ReallocIndex(uint32_t num_buckets) {
    _index = pdp::Reallocate<Index>(allocator, _index,
                                    static_cast<uint64_t>(EMH_INDEX_EAD + num_buckets));
    pdp_assert(_index);
  }

  void Rebuild(uint32_t num_buckets) {
    ReallocBucket((uint32_t)(num_buckets * MaxLoadFactor()) + 4);
    ReallocIndex(num_buckets);

    memset(_index, EMH_INDEX_INACTIVE, sizeof(_index[0]) * num_buckets);
    memset(_index + num_buckets, 0, sizeof(_index[0]) * EMH_INDEX_EAD);
  }

  void Rehash(uint64_t required_buckets) {
    if (required_buckets < _num_filled) {
      return;
    }

    pdp_assert(required_buckets < MaxBucketCount());
    uint64_t num_buckets = _num_filled > (1u << 16) ? (1u << 16) : 4u;
    while (num_buckets < required_buckets) {
      num_buckets *= 2;
    }

    _last = _mask / 4;
    _mask = num_buckets - 1;
    _num_buckets = num_buckets;

    Rebuild(num_buckets);

    _etail = EMH_INDEX_INACTIVE;
    for (uint32_t slot = 0; slot < _num_filled; ++slot) {
      const auto &key = _pairs[slot].key;
      const size_t key_hash = hasher(key);
      const uint32_t bucket = FindUniqueBucket(key_hash);
      _index[bucket] = {bucket, slot | ((uint32_t)(key_hash) & ~_mask)};
    }
  }

  // Can we fit another element?
  inline bool CheckExpandNeed() {
    uint64_t num_elems = _num_filled;
    const uint32_t required_buckets = num_elems * _mlf >> 27;
    if (PDP_LIKELY(required_buckets < _mask)) {
      return false;
    }

    Rehash(required_buckets + 2);
    return true;
  }

  uint32_t SlotToBucket(uint32_t slot) const {
    uint32_t main_bucket;
    return FindSlotBucket(slot, main_bucket);
  }

  // Very slow
  void EraseSlot(const uint32_t sbucket, const uint32_t main_bucket) {
    const uint32_t slot = _index[sbucket].slot & _mask;
    const uint32_t ebucket = EraseBucket(sbucket, main_bucket);
    const uint32_t last_slot = --_num_filled;
    if (PDP_LIKELY(slot != last_slot)) {
      const uint32_t last_bucket =
          (_etail == EMH_INDEX_INACTIVE || ebucket == _etail) ? SlotToBucket(last_slot) : _etail;

      _pairs[slot] = std::move(_pairs[last_slot]);
      _index[last_bucket].slot = slot | (_index[last_bucket].slot & ~_mask);
    }

    if (!std::is_trivially_destructible<V>::value) {
      _pairs[last_slot].~Entry();
    }

    _etail = EMH_INDEX_INACTIVE;
    _index[ebucket] = {EMH_INDEX_INACTIVE, 0};
  }

  uint32_t EraseBucket(uint32_t bucket, uint32_t main_bucket) {
    const uint32_t next_bucket = _index[bucket].next;
    if (bucket == main_bucket) {
      if (main_bucket != next_bucket) {
        const uint32_t nbucket = _index[next_bucket].next;
        _index[main_bucket] = {(nbucket == next_bucket) ? main_bucket : nbucket,
                               _index[next_bucket].slot};
      }
      return next_bucket;
    }

    const uint32_t prev_bucket = FindPrevBucket(main_bucket, bucket);
    _index[prev_bucket].next = (bucket == next_bucket) ? prev_bucket : next_bucket;
    return bucket;
  }

  // Find the slot with this key, or return bucket size
  uint32_t FindSlotBucket(uint32_t slot, uint32_t &main_bucket) const {
    const uint64_t key_hash = hasher(_pairs[slot].key);
    const uint32_t bucket = main_bucket = uint32_t(key_hash & _mask);
    if (slot == (_index[bucket].slot & _mask)) {
      return bucket;
    }

    uint32_t next_bucket = _index[bucket].next;
    while (true) {
      if (PDP_LIKELY(slot == (_index[next_bucket].slot & _mask))) {
        return next_bucket;
      }
      next_bucket = _index[next_bucket].next;
    }

    return EMH_INDEX_INACTIVE;
  }

  // Find the slot with this key, or return bucket size
  uint32_t FindFilledBucket(const K &key, uint64_t key_hash) const {
    const uint32_t bucket = key_hash & _mask;
    uint32_t next_bucket = _index[bucket].next;
    if (PDP_UNLIKELY((int)next_bucket < 0)) {
      return EMH_INDEX_INACTIVE;
    }

    if (EMH_EQHASH(bucket, key_hash)) {
      const uint32_t slot = _index[bucket].slot & _mask;
      if (PDP_LIKELY(_pairs[slot].first == key)) {
        return bucket;
      }
    }
    if (next_bucket == bucket) {
      return EMH_INDEX_INACTIVE;
    }

    while (true) {
      if (EMH_EQHASH(next_bucket, key_hash)) {
        const uint32_t slot = _index[next_bucket].slot & _mask;
        if (PDP_LIKELY(_pairs[slot].first == key)) {
          return next_bucket;
        }
      }

      const uint32_t nbucket = _index[next_bucket].next;
      if (nbucket == next_bucket) {
        return EMH_INDEX_INACTIVE;
      }
      next_bucket = nbucket;
    }

    return EMH_INDEX_INACTIVE;
  }

  // Find the slot with this key, or return bucket size
  uint32_t FindFilledSlot(const K &key) const {
    const uint64_t key_hash = hasher(key);
    const uint32_t bucket = uint32_t(key_hash & _mask);
    uint32_t next_bucket = _index[bucket].next;
    if ((int)next_bucket < 0) {
      return _num_filled;
    }

    if (EMH_EQHASH(bucket, key_hash)) {
      const uint32_t slot = _index[bucket].slot & _mask;
      if (PDP_LIKELY(_pairs[slot].key == key)) {
        return slot;
      }
    }
    if (next_bucket == bucket) {
      return _num_filled;
    }

    while (true) {
      if (EMH_EQHASH(next_bucket, key_hash)) {
        const uint32_t slot = _index[next_bucket].slot & _mask;
        if (PDP_LIKELY(_pairs[slot].key == key)) {
          return slot;
        }
      }

      const uint32_t nbucket = _index[next_bucket].next;
      if (nbucket == next_bucket) {
        return _num_filled;
      }
      next_bucket = nbucket;
    }

    return _num_filled;
  }

  // kick out bucket and find empty to occpuy
  // it will break the orgin link and relnik again.
  // before: main_bucket-->prev_bucket --> bucket   --> next_bucket
  // atfer : main_bucket-->prev_bucket --> (removed)--> new_bucket--> next_bucket
  uint32_t KickoutBucket(const uint32_t kmain, const uint32_t bucket) {
    const uint32_t next_bucket = _index[bucket].next;
    const uint32_t new_bucket = FindEmptyBucket(next_bucket);
    const uint32_t prev_bucket = FindPrevBucket(kmain, bucket);

    const uint32_t last = next_bucket == bucket ? new_bucket : next_bucket;
    _index[new_bucket] = {last, _index[bucket].slot};

    _index[prev_bucket].next = new_bucket;
    _index[bucket].next = EMH_INDEX_INACTIVE;

    return bucket;
  }

  /*
   ** inserts a new key into a hash table; first, check whether key's main
   ** bucket/position is free. If not, check whether colliding node/bucket is in its main
   ** position or not: if it is not, move colliding bucket to an empty place and
   ** put new key in its main position; otherwise (colliding bucket is in its main
   ** position), new key goes to an empty position.
   */
  uint32_t FindOrAllocate(const K &key, uint64_t key_hash) {
    const uint32_t bucket = uint32_t(key_hash & _mask);
    uint32_t next_bucket = _index[bucket].next;
    if ((int)next_bucket < 0) {
      return bucket;
    }

    const uint32_t slot = _index[bucket].slot & _mask;
    if (EMH_EQHASH(bucket, key_hash)) {
      if (PDP_LIKELY(_pairs[slot].key == key)) {
        return bucket;
      }
    }

    // check current bucket_key is in main bucket or not
    const uint32_t kmain = HashBucket(_pairs[slot].key);
    if (kmain != bucket) {
      return KickoutBucket(kmain, bucket);
    } else if (next_bucket == bucket) {
      return _index[next_bucket].next = FindEmptyBucket(next_bucket);
    }

    // find next linked bucket and check key
    while (true) {
      const uint32_t eslot = _index[next_bucket].slot & _mask;
      if (EMH_EQHASH(next_bucket, key_hash)) {
        if (PDP_LIKELY(_pairs[eslot].key == key)) {
          return next_bucket;
        }
      }

      const uint32_t nbucket = _index[next_bucket].next;
      if (nbucket == next_bucket) {
        break;
      }
      next_bucket = nbucket;
    }

    // find a empty and link it to tail
    const uint32_t new_bucket = FindEmptyBucket(next_bucket);
    return _index[next_bucket].next = new_bucket;
  }

  uint32_t FindUniqueBucket(uint64_t key_hash) {
    const uint32_t bucket = uint32_t(key_hash & _mask);
    uint32_t next_bucket = _index[bucket].next;
    if ((int)next_bucket < 0) {
      return bucket;
    }

    // check current bucket_key is in main bucket or not
    const uint32_t kmain = HashMain(bucket);
    if (PDP_UNLIKELY(kmain != bucket)) {
      return KickoutBucket(kmain, bucket);
    } else if (PDP_UNLIKELY(next_bucket != bucket)) {
      next_bucket = FindLastBucket(next_bucket);
    }

    return _index[next_bucket].next = FindEmptyBucket(next_bucket);
  }

  /***
    Different probing techniques usually provide a trade-off between memory locality and avoidance
    of clustering. Since Robin Hood hashing is relatively resilient to clustering (both primary
    and secondary), linear probing is the most cache friendly alternativeis typically used.

    It's the core algorithm of this hash map with highly optimization/benchmark.
    normaly linear probing is inefficient with high load factor, it use a new 3-way linear
    probing strategy to search empty slot. from benchmark even the load factor > 0.9, it's more
    2-3 timer fast than one-way search strategy.

    1. linear or quadratic probing a few cache line for less cache miss from input slot
    "bucket_from".
    2. the first  search  slot from member variant "_last", init with 0
    3. the second search slot from calculated pos "(_num_filled + _last) & _mask", it's like a
    rand value
    */
  // key is not in this mavalue. Find a place to put it.
  uint32_t FindEmptyBucket(uint32_t bucket_from) {
    uint32_t bucket = bucket_from;
    if (EMH_EMPTY(++bucket) || EMH_EMPTY(++bucket)) {
      return bucket;
    }

    constexpr uint32_t quadratic_probe_length = 6u;
    for (uint32_t offset = 4u, step = 3u; step < quadratic_probe_length;) {
      bucket = (bucket_from + offset) & _mask;
      if (EMH_EMPTY(bucket) || EMH_EMPTY(++bucket)) {
        return bucket;
      }
      offset += step++;
    }

    for (;;) {
      if (EMH_EMPTY(++_last)) {
        return _last;
      }

      _last &= _mask;
      uint32_t medium = (_num_buckets / 2 + _last) & _mask;
      if (EMH_EMPTY(medium)) {
        return medium;
      }
    }

    return 0;
  }

  uint32_t FindLastBucket(uint32_t main_bucket) const {
    uint32_t next_bucket = _index[main_bucket].next;
    if (next_bucket == main_bucket) {
      return main_bucket;
    }

    while (true) {
      const uint32_t nbucket = _index[next_bucket].next;
      if (nbucket == next_bucket) {
        return next_bucket;
      }
      next_bucket = nbucket;
    }
  }

  uint32_t FindPrevBucket(const uint32_t main_bucket, const uint32_t bucket) const {
    uint32_t next_bucket = _index[main_bucket].next;
    if (next_bucket == bucket) {
      return main_bucket;
    }

    while (true) {
      const uint32_t nbucket = _index[next_bucket].next;
      if (nbucket == bucket) {
        return next_bucket;
      }
      next_bucket = nbucket;
    }
  }

  inline uint32_t HashBucket(const K &key) const { return (uint32_t)hasher(key) & _mask; }

  inline uint32_t HashMain(const uint32_t bucket) const {
    const uint32_t slot = _index[bucket].slot & _mask;
    return (uint32_t)hasher(_pairs[slot].key) & _mask;
  }

 private:
  Index *_index;
  Entry *_pairs;

  uint32_t _mlf;
  uint32_t _mask;
  uint32_t _num_buckets;
  uint32_t _num_filled;
  uint32_t _last;
  uint32_t _etail;

  pdp::Hash<K> hasher;
  Alloc allocator;
};

}  // namespace emhash8

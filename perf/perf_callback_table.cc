#include <random>

#include "data/callback_table.h"
#include "external/ankerl_hash.h"
#include "external/emhash8.h"
#include "tracing/stopwatch.h"

using namespace pdp;

static constexpr uint32_t kTotalIds = 100'000;

static constexpr uint64_t seed = 0xdeadbeef;

struct AddId {
  uint64_t *out;
  uint32_t id;

  AddId(uint64_t *out, uint32_t id) : out(out), id(id) {}
  void operator()(int) { *out = ankerl::unordered_dense::mix(*out, id); }
};

template <typename T>
uint64_t StressTable(uint32_t max_active) {
  T table;

  uint64_t actual_hash = 0x9E3779B97F4A7C15;

  // Track active IDs
  std::vector<uint32_t> active;
  active.reserve(max_active);

  // Deterministic RNG (important for reproducibility)
  std::mt19937 rng(seed);

  uint32_t next_id = 1;

  size_t actual_max_active = 0;
  auto bind_one = [&]() {
    uint32_t id = next_id++;
    table.template Bind<AddId>(id, &actual_hash, id);
    active.push_back(id);
    if (active.size() > actual_max_active) {
      actual_max_active = active.size();
    }
  };

  auto invoke_random = [&]() {
    std::uniform_int_distribution<size_t> pick(0, active.size() - 1);
    size_t idx = pick(rng);

    uint32_t id = active[idx];
    table.Invoke(id, 0);

    // remove by swap+pop (creates holes & randomness)
    active[idx] = active.back();
    active.pop_back();
  };

  // Main flow
  while (next_id <= kTotalIds || !active.empty()) {
    bool can_bind = next_id <= kTotalIds && active.size() < max_active;
    bool can_invoke = !active.empty();

    // Decide action
    if (can_bind && can_invoke) {
      double load = double(active.size()) / double(max_active);
      // Controls how aggressively we stop binding near max.
      // 4–6 is a good range.
      constexpr double k = 5.0;

      double p_bind = std::exp(-k * load);

      // rng() assumed to be uint64_t
      double u = double(rng()) / double(UINT64_MAX);

      if (u < p_bind) {
        bind_one();
      } else {
        invoke_random();
      }
      // // Bias slightly toward bind to keep pressure on holes
      // if ((rng() & 1) == 0) {
      //   bind_one();
      // } else {
      //   invoke_random();
      // }
    } else if (can_bind) {
      bind_one();
    } else {
      invoke_random();
    }
  }

  if (actual_max_active < max_active) {
    pdp_warning("Actually max active: {}", actual_max_active);
  }
  return actual_hash;
}

template <typename V>
struct WeirdHashMap : private emhash8::Map<uint32_t, V> {
  using Entry = typename emhash8::Map<uint32_t, V>::Entry;
  using Index = typename emhash8::Map<uint32_t, V>::Index;

  WeirdHashMap() {
    this->_pairs = Allocate<Entry>(this->allocator, map_threshold);
    this->_index = nullptr;

    for (size_t i = 0; i < map_threshold; ++i) {
      this->_pairs[i].key = -1;
    }
  }

  template <typename... Types>
  void EmplaceUnique(uint32_t key, Types &&...args) {
    if (PDP_LIKELY(!this->_index)) {
      for (size_t i = 0; i < map_threshold; ++i) {
        if (PDP_UNLIKELY(this->_pairs[i].key == -1)) {
          this->_pairs[i].key = key;
          auto *dest = &this->_pairs[i].value;
          new (dest) V(std::forward<Types>(args)...);
          return;
        }
      }
      SwitchEmhash();
    }
    return emhash8::Map<uint32_t, V>::EmplaceUnique(key, std::forward<Types>(args)...);
    // PDP_UNREACHABLE();
  }

  Entry *Find(uint32_t key) {
    if (PDP_LIKELY(!this->_index)) {
      for (size_t i = 0; i < map_threshold; ++i) {
        if (PDP_UNLIKELY(this->_pairs[i].key == key)) {
          return &this->_pairs[i];
        }
      }
    } else {
      return emhash8::Map<uint32_t, V>::Find(key);
    }
    PDP_UNREACHABLE();
  }

  void Erase(Entry *it) {
    if (PDP_LIKELY(!this->_index)) {
      it->key = -1;
    } else {
      emhash8::Map<uint32_t, V>::Erase(it);
      if (PDP_UNLIKELY(this->_num_filled <= linear_threshold)) {
        SwitchLinear();
      }
    }
  }

 private:
  void SwitchLinear() {
    Deallocate<Index>(this->allocator, this->_index);
    this->_pairs = Reallocate<Entry>(this->allocator, this->_pairs, map_threshold);
    for (size_t i = this->_num_filled; i < map_threshold; ++i) {
      this->_pairs[i].key = -1;
    }
    this->_index = nullptr;
  }

  void SwitchEmhash() {
    // TODO eeeh
    emhash8::Map<uint32_t, V> tmp;
    for (size_t i = 0; i < map_threshold; ++i) {
      tmp.EmplaceUnique(this->_pairs[i].key, std::move(this->_pairs[i].value));
    }
    emhash8::Map<uint32_t, V>::Swap(tmp);
  }

  static constexpr unsigned map_threshold = 20;
  static constexpr unsigned linear_threshold = 5;
};

template <typename Context, typename Emhash>
struct Wrapper {
  template <typename T, typename... Args>
  void Bind(uint32_t id, Args &&...capture_args) {
    SmallCallback<Context> cb;
    new (cb.storage) T(std::forward<Args>(capture_args)...);
    cb.invoke = &SmallCallback<Context>::template InvokeImpl<T>;

    map.EmplaceUnique(id, std::move(cb));
  }

  bool Invoke(uint32_t id, Context ctx) {
    auto *entry = map.Find(id);
    entry->value(ctx);
    map.Erase(entry);
    return true;
  }

 private:
  Emhash map;
};

int main() {
  using EmHashCallback = emhash8::Map<uint32_t, SmallCallback<int>>;
  using WeirdHashCallback = WeirdHashMap<SmallCallback<int>>;

  pdp::HardwareStopwatch watch;
  for (uint32_t max_active : {5, 20, 100, 1000, 10'000, 100'000}) {
  // for (uint32_t max_active : {5}) {
    watch.LapClocks();
    uint64_t res1 = StressTable<Wrapper<int, WeirdHashCallback>>(max_active);
    auto elapsed_linear = watch.LapClocks();
    uint64_t res2 = StressTable<Wrapper<int, EmHashCallback>>(max_active);
    auto elapsed_hash = watch.LapClocks();
    if (res1 != res2) {
      PDP_UNREACHABLE();
    }
    pdp_info("Got the same result: {}", res1);
    pdp_info("Weird  took {} cycles with max_active {}", elapsed_linear, max_active);
    pdp_info("Emhash took {} cycles with max_active {}", elapsed_hash, max_active);
  }

  return 0;
}

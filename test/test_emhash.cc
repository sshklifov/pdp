#include <random>
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "strings/string_slice.h"

#include "external/emhash8.h"

using pdp::Hash;
using pdp::StringSlice;

TEST_CASE("Map<uint32_t, uint32_t>: basic empty invariants") {
  emhash8::Map<uint32_t, uint32_t> m;

  CHECK(m.Empty());
  CHECK(m.Size() == 0);

  CHECK(m.Begin() == m.End());

  auto *e = m.Find(123);
  CHECK(e == m.End());
}

TEST_CASE("Map<uint32_t, uint32_t>: Emplace inserts and Find returns same entry") {
  emhash8::Map<uint32_t, uint32_t> m;

  auto *e1 = m.Emplace(1, 111);
  REQUIRE(e1 != nullptr);
  CHECK(e1->key == 1);
  CHECK(e1->value == 111);

  CHECK(!m.Empty());
  CHECK(m.Size() == 1);

  auto *f1 = m.Find(1);
  REQUIRE(f1 != m.End());
  CHECK(f1->key == 1);
  CHECK(f1->value == 111);

  // Emplace same key should not create a new element; should return existing slot.
  auto *e1b = m.Emplace(1, 999);
  REQUIRE(e1b != nullptr);
  CHECK(m.Size() == 1);
  CHECK(e1b->key == 1);
  CHECK(e1b->value == 111);
}

TEST_CASE("Map<uint32_t, uint32_t>: multiple inserts, iteration covers all keys") {
  emhash8::Map<uint32_t, uint32_t> m;

  constexpr uint32_t N = 200;
  for (uint32_t i = 0; i < N; ++i) {
    auto *e = m.Emplace(i, i + 10);
    REQUIRE(e != nullptr);
  }
  CHECK(m.Size() == N);

  // Verify Find for all.
  for (uint32_t i = 0; i < N; ++i) {
    auto *e = m.Find(i);
    REQUIRE(e != m.End());
    CHECK(e->key == i);
    CHECK(e->value == i + 10);
  }

  // Verify iteration hits exactly N elements.
  uint32_t count = 0;
  for (auto it = m.Begin(); it != m.End(); ++it) {
    ++count;
  }
  CHECK(count == N);
}

TEST_CASE("Map<uint32_t, uint32_t>: Erase missing key returns false") {
  emhash8::Map<uint32_t, uint32_t> m;

  m.Emplace(1, 10);
  m.Emplace(2, 20);

  CHECK(m.Size() == 2);

  CHECK(m.Erase(999) == false);
  CHECK(m.Size() == 2);
  CHECK(m.Find(1) != m.End());
  CHECK(m.Find(2) != m.End());
}

TEST_CASE("Map<uint32_t, uint32_t>: Erase by key removes element and keeps table consistent") {
  emhash8::Map<uint32_t, uint32_t> m;

  constexpr uint32_t N = 300;
  for (uint32_t i = 0; i < N; ++i) {
    m.Emplace(i, i * 3);
  }
  CHECK(m.Size() == N);

  // Remove evens.
  for (uint32_t i = 0; i < N; i += 2) {
    CHECK(m.Erase(i) == true);
  }
  CHECK(m.Size() == N / 2);

  // Evens gone, odds present.
  for (uint32_t i = 0; i < N; ++i) {
    auto *e = m.Find(i);
    if ((i % 2) == 0) {
      CHECK(e == m.End());
    } else {
      REQUIRE(e != m.End());
      CHECK(e->key == i);
      CHECK(e->value == i * 3);
    }
  }

  // Now delete the remaining odds too.
  for (uint32_t i = 1; i < N; i += 2) {
    CHECK(m.Erase(i) == true);
  }
  CHECK(m.Empty());
  CHECK(m.Size() == 0);
  CHECK(m.Begin() == m.End());
}

TEST_CASE("Map<uint32_t, uint32_t>: Erase by iterator removes that entry") {
  emhash8::Map<uint32_t, uint32_t> m;

  for (uint32_t i = 0; i < 50; ++i) {
    m.Emplace(i, i + 1000);
  }
  CHECK(m.Size() == 50);

  // Pick some existing entry pointer and erase it.
  auto *e = m.Find(17);
  REQUIRE(e != m.End());
  REQUIRE(e->key == 17);

  m.Erase(e);

  CHECK(m.Size() == 49);
  CHECK(m.Find(17) == m.End());

  // Remaining still valid.
  for (uint32_t i = 0; i < 50; ++i) {
    if (i == 17) continue;
    auto *f = m.Find(i);
    REQUIRE(f != m.End());
    CHECK(f->key == i);
    CHECK(f->value == i + 1000);
  }
}

TEST_CASE("Map<uint32_t, uint32_t>: Clear resets size but keeps container usable") {
  emhash8::Map<uint32_t, uint32_t> m;

  for (uint32_t i = 0; i < 100; ++i) {
    m.Emplace(i, i);
  }
  CHECK(m.Size() == 100);

  m.Clear();
  CHECK(m.Empty());
  CHECK(m.Size() == 0);
  CHECK(m.Begin() == m.End());

  // Reuse after clear.
  m.Emplace(42, 4242);
  CHECK(!m.Empty());
  CHECK(m.Size() == 1);
  auto *e = m.Find(42);
  REQUIRE(e != m.End());
  CHECK(e->value == 4242);
}

TEST_CASE("Map<uint32_t, uint32_t>: Swap swaps contents") {
  emhash8::Map<uint32_t, uint32_t> a;
  emhash8::Map<uint32_t, uint32_t> b;

  for (uint32_t i = 0; i < 10; ++i) a.Emplace(i, i + 1);
  for (uint32_t i = 100; i < 120; ++i) b.Emplace(i, i + 2);

  a.Swap(b);

  CHECK(b.Size() == 10);
  for (uint32_t i = 0; i < 10; ++i) {
    auto it = b.Find(i);
    CHECK(it != b.End());
    uint32_t j = i + 1;
    CHECK(it->value == j);
  }
  CHECK(a.Size() == 20);
  for (uint32_t i = 100; i < 120; ++i) {
    auto it = a.Find(i);
    CHECK(it != a.End());
    uint32_t j = i + 2;
    CHECK(it->value == j);
  }
}

TEST_CASE("Map<uint32_t, uint32_t>: move ctor and move assignment preserve values") {
  pdp::TrackingAllocator::Stats stats;

  emhash8::Map<uint32_t, uint32_t, pdp::TrackingAllocator> m(&stats);
  for (uint32_t i = 0; i < 100; ++i) m.Emplace(i, i * 7);

  auto was_bytes = stats.GetBytesUsed();

  emhash8::Map<uint32_t, uint32_t, pdp::TrackingAllocator> moved(std::move(m));
  CHECK(moved.Size() == 100);

  auto now_bytes = stats.GetBytesUsed();
  CHECK(was_bytes == now_bytes);

  for (uint32_t i = 0; i < 100; ++i) {
    auto *e = moved.Find(i);
    REQUIRE(e != moved.End());
    CHECK(e->value == i * 7);
  }

  emhash8::Map<uint32_t, uint32_t, pdp::TrackingAllocator> assigned(&stats);
  was_bytes = stats.GetBytesUsed();

  assigned = std::move(moved);
  CHECK(assigned.Size() == 100);

  now_bytes = stats.GetBytesUsed();
  CHECK(was_bytes == now_bytes);

  for (uint32_t i = 0; i < 100; ++i) {
    auto *e = assigned.Find(i);
    REQUIRE(e != assigned.End());
    CHECK(e->value == i * 7);
  }
}

TEST_CASE("Map<uint32_t, uint32_t>: high load / rehash stress") {
  emhash8::Map<uint32_t, uint32_t> m;

  // Push enough to force multiple expansions.
  constexpr uint32_t N = 5000;
  for (uint32_t i = 0; i < N; ++i) {
    m.Emplace(i, i ^ 0xA5A5A5A5u);
  }
  CHECK(m.Size() == N);

  // Random-ish access pattern.
  for (uint32_t i = 0; i < N; i += 3) {
    auto *e = m.Find(i);
    REQUIRE(e != m.End());
    CHECK(e->key == i);
    CHECK(e->value == (i ^ 0xA5A5A5A5u));
  }

  // Remove a chunk, then insert again.
  for (uint32_t i = 0; i < N; i += 4) {
    CHECK(m.Erase(i) == true);
  }
  CHECK(m.Size() == N - (N + 3) / 4);

  for (uint32_t i = 0; i < N; i += 4) {
    auto *e = m.Emplace(i, 123456u);
    REQUIRE(e != nullptr);
  }
  CHECK(m.Size() == N);

  for (uint32_t i = 0; i < N; ++i) {
    auto *e = m.Find(i);
    REQUIRE(e != m.End());
    if ((i % 4) == 0)
      CHECK(e->value == 123456u);
    else
      CHECK(e->value == (i ^ 0xA5A5A5A5u));
  }
}

// ------------------ StringSlice keys ------------------

TEST_CASE("Map<StringSlice, uint32_t>: basic insert/find with stable backing storage") {
  // IMPORTANT: StringSlice is a view. Keys must reference stable memory.
  // We use string literals and std::string storage that outlives the map usage.
  emhash8::Map<StringSlice, uint32_t> m;

  StringSlice a("alpha");
  StringSlice b("beta");
  StringSlice c("gamma");

  m.Emplace(a, 1u);
  m.Emplace(b, 2u);
  m.Emplace(c, 3u);

  CHECK(m.Size() == 3);

  auto *ea = m.Find(StringSlice("alpha"));
  REQUIRE(ea != m.End());
  CHECK(ea->key == a);
  CHECK(ea->value == 1u);

  auto *eb = m.Find(StringSlice("beta"));
  REQUIRE(eb != m.End());
  CHECK(eb->value == 2u);

  auto *ex = m.Find(StringSlice("does-not-exist"));
  CHECK(ex == m.End());
}

TEST_CASE("Map<StringSlice, uint32_t>: collisions & erase") {
  emhash8::Map<StringSlice, uint32_t> m;

  // Make lots of short keys.
  std::vector<std::string> storage;
  storage.reserve(1000);

  for (int i = 0; i < 1000; ++i) {
    storage.push_back("k" + std::to_string(i));
    m.Emplace(StringSlice(storage.back().c_str()), i);
  }
  CHECK(m.Size() == 1000);

  // Remove every 5th key.
  for (int i = 0; i < 1000; i += 5) {
    StringSlice k(storage[i].c_str());
    CHECK(m.Erase(k) == true);
  }
  CHECK(m.Size() == 1000 - 200);

  // Verify.
  for (int i = 0; i < 1000; ++i) {
    StringSlice k(storage[i].c_str());
    auto *e = m.Find(k);
    if (i % 5 == 0) {
      CHECK(e == m.End());
    } else {
      REQUIRE(e != m.End());
      CHECK(e->value == (uint32_t)i);
    }
  }
}

TEST_CASE("Map<StringSlice, uint32_t>: Clear and reuse") {
  emhash8::Map<StringSlice, uint32_t> m;

  std::string s1 = "hello";
  std::string s2 = "world";

  m.Emplace(StringSlice(s1.c_str()), 10u);
  m.Emplace(StringSlice(s2.c_str()), 20u);
  CHECK(m.Size() == 2);

  m.Clear();
  CHECK(m.Empty());

  std::string s3 = "again";
  m.Emplace(StringSlice(s3.c_str()), 30u);

  CHECK(m.Size() == 1);
  auto *e = m.Find(StringSlice("again"));
  REQUIRE(e != m.End());
  CHECK(e->value == 30u);
}

TEST_CASE("Map<StringSlice, uint32_t>: Swap / move semantics") {
  emhash8::Map<StringSlice, uint32_t> a;
  emhash8::Map<StringSlice, uint32_t> b;

  std::string a1 = "a1", a2 = "a2";
  std::string b1 = "b1", b2 = "b2", b3 = "b3";

  a.Emplace(StringSlice(a1.c_str()), 1u);
  a.Emplace(StringSlice(a2.c_str()), 2u);

  b.Emplace(StringSlice(b1.c_str()), 10u);
  b.Emplace(StringSlice(b2.c_str()), 20u);
  b.Emplace(StringSlice(b3.c_str()), 30u);

  a.Swap(b);

  CHECK(a.Size() == 3);
  CHECK(b.Size() == 2);

  CHECK(a.Find(StringSlice("b1")) != a.End());
  CHECK(a.Find(StringSlice("a1")) == a.End());

  emhash8::Map<StringSlice, uint32_t> c(std::move(a));
  CHECK(c.Size() == 3);
  CHECK(c.Find(StringSlice("b2")) != c.End());

  emhash8::Map<StringSlice, uint32_t> d;
  d = std::move(c);
  CHECK(d.Size() == 3);
  CHECK(d.Find(StringSlice("b3")) != d.End());
}

// ------------------ HASH test ------------------

namespace {

constexpr size_t NUM_SAMPLES = 200'000;
constexpr size_t NUM_BUCKETS = 4096;  // realistic hash table size

template <typename T, typename Gen>
double ChiSquaredForHash(Gen &&generator) {
  std::vector<uint64_t> buckets(NUM_BUCKETS, 0);
  Hash<T> hash;

  for (size_t i = 0; i < NUM_SAMPLES; ++i) {
    T value = generator();
    uint64_t h = hash(value);
    buckets[h & (NUM_BUCKETS - 1)]++;
  }

  const double expected = double(NUM_SAMPLES) / NUM_BUCKETS;
  double chi2 = 0.0;

  for (uint64_t obs : buckets) {
    double diff = obs - expected;
    chi2 += (diff * diff) / expected;
  }

  return chi2 / NUM_BUCKETS;  // normalized
}

constexpr size_t NUM_COARSE_BUCKETS = 20;

// Deliberately skewed weights (sum doesn't matter)
static constexpr double bucket_weights[NUM_COARSE_BUCKETS] = {
    50.0, 30.0, 20.0, 15.0, 10.0, 5.0, 4.0, 3.0, 2.5, 2.0,
    1.8,  1.6,  1.4,  1.2,  1.0,  0.8, 0.6, 0.4, 0.2, 0.1};

constexpr uint64_t BUCKET_SPAN = 1ull << 16;  // 65k values per bucket

struct SkewedBucketGenerator {
  std::mt19937_64 rng;
  std::discrete_distribution<size_t> bucket_dist;
  std::uniform_int_distribution<uint64_t> offset_dist;

  SkewedBucketGenerator()
      : rng(123),
        bucket_dist(std::begin(bucket_weights), std::end(bucket_weights)),
        offset_dist(0, BUCKET_SPAN - 1) {}

  uint64_t operator()() {
    const size_t bucket = bucket_dist(rng);
    const uint64_t base = uint64_t(bucket) * BUCKET_SPAN;
    return base + offset_dist(rng);
  }
};

}  // namespace

// -------------------- uint64_t hash --------------------

TEST_CASE("Hash<uint64_t>: uniform input") {
  std::mt19937_64 rng(123);
  std::uniform_int_distribution<uint64_t> dist;

  double chi2 = ChiSquaredForHash<uint64_t>([&]() { return dist(rng); });

  auto s = std::to_string(chi2);
  pdp_warning("Chi2 of Hash<uint64_t> with uniform: {}", StringSlice(s.c_str()));
  CHECK_MESSAGE(chi2 < 2.0, "Chi2 too high for uniform input: " << chi2);
}

TEST_CASE("Hash<uint64_t>: normal distribution input") {
  std::mt19937_64 rng(123);
  std::normal_distribution<double> dist(0.0, 1e6);

  double chi2 = ChiSquaredForHash<uint64_t>([&]() { return static_cast<uint64_t>(dist(rng)); });

  auto s = std::to_string(chi2);
  pdp_warning("Chi2 of Hash<uint64_t> with normal(0, 1e6): {}", StringSlice(s.c_str()));
  CHECK_MESSAGE(chi2 < 2.5, "Chi2 too high for normal input: " << chi2);
}

TEST_CASE("Hash<uint64_t>: skewed coarse-bucket distribution") {
  SkewedBucketGenerator gen;

  double chi2 = ChiSquaredForHash<uint64_t>([&]() { return gen(); });
  auto s = std::to_string(chi2);
  pdp_warning("Chi2 of Hash<uint64_t> with skewed coarse-bucket: {}", StringSlice(s.c_str()));

  CHECK_MESSAGE(chi2 < 3.0, "Chi² too high for skewed bucket generator: " << chi2);
}

// -------------------- StringSlice hash --------------------

TEST_CASE("Hash<StringSlice>: uniform random strings") {
  std::mt19937 rng(123);
  std::uniform_int_distribution<int> len_dist(4, 32);
  std::uniform_int_distribution<char> char_dist('a', 'z');

  std::vector<std::string> storage;
  storage.reserve(NUM_SAMPLES);

  double chi2 = ChiSquaredForHash<StringSlice>([&]() -> StringSlice {
    int len = len_dist(rng);
    std::string s;
    s.reserve(len);
    for (int i = 0; i < len; ++i) s.push_back(char_dist(rng));
    storage.push_back(s);
    return StringSlice(storage.back().c_str(), storage.back().size());
  });

  auto s = std::to_string(chi2);
  pdp_warning("Chi2 of Hash<StringSlice> with uniform: {}", StringSlice(s.c_str()));
  CHECK_MESSAGE(chi2 < 2.5, "Chi2 too high for random strings: " << chi2);
}

#if 0
TEST_CASE("Hash<StringSlice>: geometric length distribution") {
  std::mt19937 rng(123);
  std::geometric_distribution<int> len_dist(0.1);
  std::uniform_int_distribution<char> char_dist('a', 'z');

  std::vector<std::string> storage;
  storage.reserve(NUM_SAMPLES);

  double chi2 = ChiSquaredForHash<StringSlice>([&]() -> StringSlice {
    int len = std::max(1, std::min(len_dist(rng), 64));
    std::string s;
    s.reserve(len);
    for (int i = 0; i < len; ++i) s.push_back(char_dist(rng));
    storage.push_back(s);
    return StringSlice(storage.back().c_str(), storage.back().size());
  });

  auto s = std::to_string(chi2);
  pdp_warning("Chi2 of Hash<StringSlice> with geometric(0.1): {}", StringSlice(s.c_str()));
  CHECK_MESSAGE(chi2 < 3.0, "Chi2 too high for geometric-length strings: " << chi2);
}
#endif

TEST_CASE("Hash<StringSlice>: pathological similar prefixes") {
  std::vector<std::string> storage;
  storage.reserve(NUM_SAMPLES);

  size_t counter = 0;
  double chi2 = ChiSquaredForHash<StringSlice>([&]() -> StringSlice {
    storage.push_back("prefix_" + std::to_string(counter++));
    const auto &s = storage.back();
    return StringSlice(s.c_str(), s.size());
  });

  auto s = std::to_string(chi2);
  pdp_warning("Chi2 of Hash<StringSlice> with similar-prefix strings: {}", StringSlice(s.c_str()));
  CHECK_MESSAGE(chi2 < 3.0, "Chi2 too high for similar-prefix strings: " << chi2);
}

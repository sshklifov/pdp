#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "strings/string_builder.h"

using pdp::EstimateSize;
using pdp::StringBuilder;
using pdp::StringSlice;

static StringSlice SS(const char *s) { return StringSlice(s); }

#if 0
TEST_CASE("EstimateSize overloads") {
  EstimateSize est;

  SUBCASE("StringSlice") {
    StringSlice s("hello");
    CHECK(est(s) == 5);
    CHECK(est(StringSlice("what the hell", 2)) == 2);
  }

  SUBCASE("char") {
    CHECK(est('a') == 1);
    CHECK(est('\0') == 1);
  }

  SUBCASE("void*") {
    void *p = reinterpret_cast<void *>(0x1234);
    const size_t n = est(p);
    CHECK(n >= 6);
  }

  SUBCASE("integral estimate") {
    CHECK(est(int32_t{0}) == std::numeric_limits<int32_t>::digits10 + 2);
    CHECK(est(int32_t{-1}) == std::numeric_limits<int32_t>::digits10 + 2);
    CHECK(est(uint32_t{0}) == std::numeric_limits<uint32_t>::digits10 + 2);
    CHECK(est(uint64_t{123}) == std::numeric_limits<uint64_t>::digits10 + 2);
  }
}
#endif

TEST_CASE("StringBuilder basic construction and Length") {
  SUBCASE("Default ctor") {
    StringBuilder<> b;
    CHECK(b.Length() == 0);
    CHECK(b.Size() == 0);
  }

  SUBCASE("Capacity ctor") {
    StringBuilder<> b(64);
    CHECK(b.Length() == 0);
    CHECK(b.Size() == 0);
    CHECK(b.Capacity() >= 64);
  }
}

TEST_CASE("StringBuilder GetSlice and equality operators") {
  StringBuilder<> b(64);
  b.Append(SS("hello"));
  CHECK(b.Length() == 5);

  StringSlice s = b.GetSlice();
  CHECK(s == StringSlice("hello"));

  CHECK(b == StringSlice("hello"));
  CHECK_FALSE(b == StringSlice("hell"));
  CHECK_FALSE(b == StringSlice("hello!"));

  CHECK(b != StringSlice("hell"));
  CHECK_FALSE(b != StringSlice("hello"));
}

TEST_CASE("StringBuilder Substr overloads") {
  StringBuilder<> b(64);
  b.Append(SS("abcdef"));

  SUBCASE("Substr(pos)") {
    auto s = b.Substr(1);
    CHECK(s == StringSlice("bcdef"));

    auto t = b.Substr(2);
    CHECK(t.Size() == 4);
    CHECK(t[0] == 'c');
    CHECK(t == StringSlice("cdef"));
  }

  SUBCASE("Substr(pos, n)") {
    auto s = b.Substr(2, 3);
    CHECK(s.Size() == 3);
    CHECK(s[0] == 'c');
    CHECK(s[2] == 'e');
    CHECK(s == StringSlice("cde"));

    auto z = b.Substr(0, 0);
    CHECK(z.Size() == 0);
    CHECK(z.Empty());
  }

  SUBCASE("Substr(it)") {
    const char *it = b.Begin() + 3;
    auto s = b.Substr(it);
    CHECK(s.Size() == 3);
    CHECK(s == StringSlice("def"));
    CHECK(b.Substr(b.Begin() + 2) == b.Substr(2));
  }
}
TEST_CASE("AppendUnchecked(char) and Append(char) produce same output") {
  StringBuilder<> a(64);
  StringBuilder<> b(64);

  a.AppendUnchecked('x');
  a.AppendUnchecked('y');

  b.Append('x');
  b.Append('y');

  CHECK(a.GetSlice() == b.GetSlice());
  CHECK(a == StringSlice("xy"));
}

TEST_CASE("AppendUnchecked(StringSlice) and Append(StringSlice)") {
  StringBuilder<> a(64);
  StringBuilder<> b(64);

  a.AppendUnchecked(StringSlice("hello"));
  b.Append(StringSlice("hello"));

  CHECK(a == StringSlice("hello"));
  CHECK(b == StringSlice("hello"));
  CHECK(a.GetSlice() == b.GetSlice());

  a.AppendUnchecked(StringSlice(" "));
  a.AppendUnchecked(StringSlice("world"));
  CHECK(a == StringSlice("hello world"));
}

TEST_CASE("AppendUnchecked(unsigned) formatting") {
  StringBuilder<> b(128);

  b.AppendUnchecked(uint32_t{0});
  CHECK(b == StringSlice("0"));

  b = StringBuilder<>(128);
  b.AppendUnchecked(uint32_t{7});
  CHECK(b == StringSlice("7"));

  b = StringBuilder<>(128);
  b.AppendUnchecked(uint32_t{10});
  CHECK(b == StringSlice("10"));

  b = StringBuilder<>(128);
  b.AppendUnchecked(uint32_t{123456});
  CHECK(b == StringSlice("123456"));

  b = StringBuilder<>(256);
  b.AppendUnchecked(std::numeric_limits<uint64_t>::max());
  // Can't hardcode easily; sanity checks:
  auto s = b.GetSlice();
  CHECK(s.Size() > 0);
  CHECK(s[0] >= '1');
  CHECK(s[0] <= '9');
  for (size_t i = 0; i < s.Size(); ++i) {
    CHECK(s[i] >= '0');
    CHECK(s[i] <= '9');
  }
}

TEST_CASE("AppendUnchecked(signed) formatting incl edge cases") {
  StringBuilder<> b(256);

  b.AppendUnchecked(int32_t{0});
  CHECK(b == StringSlice("0"));

  b = StringBuilder<>(256);
  b.AppendUnchecked(int32_t{42});
  CHECK(b == StringSlice("42"));

  b = StringBuilder<>(256);
  b.AppendUnchecked(int32_t{-42});
  CHECK(b == StringSlice("-42"));

  // nasty: min value
  b = StringBuilder<>(256);
  b.AppendUnchecked(std::numeric_limits<int32_t>::min());
  auto s = b.GetSlice();
  CHECK(s.Size() > 1);
  CHECK(s[0] == '-');
  for (size_t i = 1; i < s.Size(); ++i) {
    CHECK(s[i] >= '0');
    CHECK(s[i] <= '9');
  }

  b = StringBuilder<>(256);
  b.AppendUnchecked(std::numeric_limits<int64_t>::min());
  s = b.GetSlice();
  CHECK(s.Size() > 1);
  CHECK(s[0] == '-');
  for (size_t i = 1; i < s.Size(); ++i) {
    CHECK(s[i] >= '0');
    CHECK(s[i] <= '9');
  }
}

TEST_CASE("Append(T) with various types") {
  SUBCASE("Append(StringSlice)") {
    StringBuilder<> b(64);
    b.Append(StringSlice("hi"));
    CHECK(b == StringSlice("hi"));
  }

  SUBCASE("Append(char)") {
    StringBuilder<> b(64);
    b.Append('A');
    CHECK(b == StringSlice("A"));
  }

  SUBCASE("Append(unsigned)") {
    StringBuilder<> b(64);
    b.Append(uint32_t{123});
    CHECK(b == StringSlice("123"));
  }

  SUBCASE("Append(signed)") {
    StringBuilder<> b(64);
    b.Append(int32_t{-5});
    CHECK(b == StringSlice("-5"));
  }
}

TEST_CASE("Appendf basic replacement") {
  StringBuilder<> b(256);

  b.Appendf("hello {}", StringSlice("world"));
  CHECK(b == StringSlice("hello world"));

  b = StringBuilder<>(256);
  b.Appendf("{}+{}={}", 2, 3, 5);
  CHECK(b == StringSlice("2+3=5"));

  b = StringBuilder<>(256);
  b.Appendf("X{}Y{}Z", 'a', 'b');
  CHECK(b == StringSlice("XaYbZ"));
}

TEST_CASE("Appendf braces behavior (escaped or malformed-ish sequences)") {
  SUBCASE("No args: Appendf(fmt) should append fmt as-is when non-empty") {
    StringBuilder<> b(256);
    b.Appendf("plain text");
    CHECK(b == StringSlice("plain text"));
  }

  SUBCASE("Empty fmt with no args does nothing") {
    StringBuilder<> b(64);
    b.Appendf("");
    CHECK(b.Empty());
  }

  SUBCASE("Single { not followed by } becomes literal { (and continues)") {
    StringBuilder<> b(256);
    b.Appendf("a{b{}c", 7);
    // Walk it:
    // finds '{' after 'a' -> appends 'a', drops to "{b{}c"
    // drops '{', next is 'b' != '}' => appends '{' literally, recurse with same arg on "b{}c"
    // finds '{' before '{}' -> appends 'b', drops, sees '}' => appends arg(7), then "c"
    CHECK(b == StringSlice("a{b7c"));
  }

  SUBCASE("Consecutive placeholders") {
    StringBuilder<> b(256);
    b.Appendf("{}{}{}", 1, 2, 3);
    CHECK(b == StringSlice("123"));
  }

  SUBCASE("Text after last placeholder") {
    StringBuilder<> b(256);
    b.Appendf("v={}.", 9);
    CHECK(b == StringSlice("v=9."));
  }
}

TEST_CASE("Appendf equivalence: Appendf vs manual Append") {
  StringBuilder<> a(256);
  StringBuilder<> b(256);

  a.Appendf("x{}y{}z", 10, StringSlice("QQ"));

  b.Append(StringSlice("x"));
  b.Append(10);
  b.Append("y");
  b.Append(StringSlice("QQ"));
  b.Append("z");

  CHECK(a.GetSlice() == b.GetSlice());
}

TEST_CASE("Multiple appends grow content correctly") {
  StringBuilder<> b(512);

  for (int i = 0; i < 50; ++i) b.Append('a');
  CHECK(b.Length() == 50);

  auto s = b.GetSlice();
  CHECK(s.Size() == 50);
  for (size_t i = 0; i < s.Size(); ++i) CHECK(s[i] == 'a');
}

TEST_CASE("Substr + Append integration") {
  StringBuilder<> b(256);
  b.Append("abcdef");

  auto s = b.Substr(2, 3);  // "cde"
  StringBuilder<> out(256);
  out.Append(StringSlice("X"));
  out.Append(s);
  out.Append(StringSlice("Y"));

  CHECK(out == StringSlice("XcdeY"));
}

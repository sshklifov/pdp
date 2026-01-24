#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "strings/string_builder.h"

using pdp::StringBuilder;
using pdp::StringSlice;

TEST_CASE("CountDigits10 basic values") {
  CHECK(pdp::CountDigits10(0) == 1);
  CHECK(pdp::CountDigits10(1) == 1);
  CHECK(pdp::CountDigits10(9) == 1);
  CHECK(pdp::CountDigits10(10) == 2);
  CHECK(pdp::CountDigits10(99) == 2);
  CHECK(pdp::CountDigits10(100) == 3);
  CHECK(pdp::CountDigits10(12345) == 5);
}

TEST_CASE("CountDigits10 matches reference for wide range") {
  for (uint64_t n = 0; n < 1'000'000; ++n) {
    auto run = pdp::CountDigits10(n);
    uint32_t ref = 0;
    uint32_t m = n;
    do {
      ref++;
      m /= 10;
    } while (m > 0);
    CHECK(run == ref);
  }
}

TEST_CASE("StringBuilder basic construction and Length") {
  SUBCASE("Default ctor") {
    StringBuilder<> b;
    CHECK(b.Length() == 0);
    CHECK(b.Size() == 0);
  }
}

TEST_CASE("CountDigits16 basic values") {
  CHECK(pdp::CountDigits16(0x0) == 1);
  CHECK(pdp::CountDigits16(0x1) == 1);
  CHECK(pdp::CountDigits16(0xF) == 1);
  CHECK(pdp::CountDigits16(0x10) == 2);
  CHECK(pdp::CountDigits16(0xFF) == 2);
  CHECK(pdp::CountDigits16(0x100) == 3);
  CHECK(pdp::CountDigits16(0x12345) == 5);
}

TEST_CASE("CountDigits16 matches reference for wide range") {
  for (uint64_t n = 0; n < 1'000'000; ++n) {
    auto run = pdp::CountDigits16(n);
    uint32_t ref = 0;
    uint64_t m = n;
    do {
      ref++;
      m /= 16;
    } while (m > 0);

    CHECK(run == ref);
  }
}

TEST_CASE("StringBuilder GetSlice") {
  StringBuilder<> b;
  b.Append(StringSlice("hello"));
  CHECK(b.Length() == 5);

  StringSlice s = b.ToSlice();
  CHECK(s == StringSlice("hello"));

  b.Append(' ');
  b.Append(5);
  CHECK(b.ToSlice() == StringSlice("hello 5"));
}

TEST_CASE("AppendUnchecked(char) and Append(char) produce same output") {
  StringBuilder<> a;
  StringBuilder<> b;
  a.ReserveFor(1);
  b.ReserveFor(1);

  a.AppendUnchecked('x');
  a.AppendUnchecked('y');

  b.Append('x');
  b.Append('y');

  CHECK(a.ToSlice() == b.ToSlice());
  CHECK(a.ToSlice() == StringSlice("xy"));
  CHECK(b.ToSlice() == StringSlice("xy"));
}

TEST_CASE("AppendUnchecked(StringSlice) and Append(StringSlice)") {
  StringBuilder<> a;
  StringBuilder<> b;
  a.ReserveFor(64);
  b.ReserveFor(64);

  a.AppendUnchecked(StringSlice("hello"));
  b.Append(StringSlice("hello"));

  CHECK(a.ToSlice() == StringSlice("hello"));
  CHECK(b.ToSlice() == StringSlice("hello"));
  CHECK(a.ToSlice() == b.ToSlice());

  a.AppendUnchecked(StringSlice(" "));
  a.AppendUnchecked(StringSlice("world"));
  CHECK(a.ToSlice() == StringSlice("hello world"));
}

TEST_CASE("AppendUnchecked(unsigned) formatting") {
  StringBuilder<> b;
  b.ReserveFor(128);

  b.AppendUnchecked(uint32_t{0});
  CHECK(b.ToSlice() == StringSlice("0"));

  b.Clear();
  b.AppendUnchecked(uint32_t{7});
  CHECK(b.ToSlice() == StringSlice("7"));

  b.Clear();
  b.AppendUnchecked(uint32_t{10});
  CHECK(b.ToSlice() == StringSlice("10"));

  b.Clear();
  b.AppendUnchecked(uint32_t{123456});
  CHECK(b.ToSlice() == StringSlice("123456"));
}

TEST_CASE("AppendUnchecked(signed) formatting incl edge cases") {
  StringBuilder<> b;
  b.ReserveFor(128);

  b.AppendUnchecked(int32_t{0});
  CHECK(b.ToSlice() == StringSlice("0"));

  b.Clear();
  b.AppendUnchecked(int32_t{42});
  CHECK(b.ToSlice() == StringSlice("42"));

  b.Clear();
  b.AppendUnchecked(int32_t{-42});
  CHECK(b.ToSlice() == StringSlice("-42"));

  // nasty: min value
  b.Clear();
  b.AppendUnchecked(std::numeric_limits<int32_t>::min());
  auto s = b.ToSlice();
  CHECK(s.Size() == 11);
  CHECK(s == "-2147483648");

  b.Clear();
  b.AppendUnchecked(std::numeric_limits<int64_t>::min());
  s = b.ToSlice();
  CHECK(s == "-9223372036854775808");
}

TEST_CASE("AppendUnchecked recursion check") {
  SUBCASE("Append(char*)") {
    char asd[] = "asd";
    StringBuilder<> b;
    b.ReserveFor(20);
    b.AppendUnchecked((char *)asd);
    CHECK(b.ToSlice() == StringSlice("asd"));
  }

  SUBCASE("AppendUnchecked(char[])") {
    char digits[] = "0123456789";
    StringBuilder<> b;
    b.ReserveFor(20);
    b.AppendUnchecked(digits);
    CHECK(b.ToSlice() == StringSlice("0123456789"));
  }

  SUBCASE("AppendUnchecked(const char*)") {
    const char *def = "def";
    StringBuilder<> b;
    b.ReserveFor(20);
    b.AppendUnchecked(def);
    CHECK(b.ToSlice() == StringSlice("def"));
  }

  SUBCASE("AppendUnchecked(const char)") {
    const char bracket = '{';
    StringBuilder<> b;
    b.ReserveFor(20);
    b.AppendUnchecked(bracket);
    CHECK(b.ToSlice() == StringSlice("{"));
  }
}

TEST_CASE("Append(T) with various types") {
  SUBCASE("Append(StringSlice)") {
    StringBuilder<> b;
    b.Append(StringSlice("hi"));
    CHECK(b.ToSlice() == StringSlice("hi"));
  }

  SUBCASE("Append(char)") {
    StringBuilder<> b;
    b.Append('A');
    CHECK(b.ToSlice() == StringSlice("A"));
  }

  SUBCASE("Append(unsigned)") {
    StringBuilder<> b;
    b.Append(uint32_t{123});
    CHECK(b.ToSlice() == StringSlice("123"));
  }

  SUBCASE("Append(signed)") {
    StringBuilder<> b;
    b.Append(int32_t{-5});
    CHECK(b.ToSlice() == StringSlice("-5"));
  }

  SUBCASE("Append(pointer)") {
    void *ptr = reinterpret_cast<void *>(0xdeadbeef);
    StringBuilder<> b;
    b.Append(ptr);
    CHECK(b.ToSlice() == StringSlice("0xdeadbeef"));
  }

  SUBCASE("Append(char*)") {
    char asd[] = "asd";
    StringBuilder<> b;
    b.Append((char *)asd);
    CHECK(b.ToSlice() == StringSlice("asd"));
  }

  SUBCASE("Append(char[])") {
    char digits[] = "0123456789";
    StringBuilder<> b;
    b.Append(digits);
    CHECK(b.ToSlice() == StringSlice("0123456789"));
  }

  SUBCASE("Append(const char*)") {
    const char *def = "def";
    StringBuilder<> b;
    b.Append(def);
    CHECK(b.ToSlice() == StringSlice("def"));
  }

  SUBCASE("Append(const char)") {
    const char bracket = '{';
    StringBuilder<> b;
    b.Append(bracket);
    CHECK(b.ToSlice() == StringSlice("{"));
  }
}

TEST_CASE("Appendf basic replacement") {
  StringBuilder<> b;

  b.AppendFormat("hello {}", StringSlice("world"));
  CHECK(b.ToSlice() == StringSlice("hello world"));

  b.Clear();
  b.AppendFormat("{}+{}={}", 2, 3, 5);
  CHECK(b.ToSlice() == StringSlice("2+3=5"));

  b.Clear();
  b.AppendFormat("X{}Y{}Z", 'a', 'b');
  CHECK(b.ToSlice() == StringSlice("XaYbZ"));
}

TEST_CASE("Appendf braces behavior (escaped or malformed-ish sequences)") {
  SUBCASE("No args: Appendf(fmt) should append fmt as-is when non-empty") {
    StringBuilder<> b;
    b.AppendFormat("plain text");
    CHECK(b.ToSlice() == StringSlice("plain text"));
  }

  SUBCASE("Empty fmt with no args does nothing") {
    StringBuilder<> b;
    b.ReserveFor(200);
    b.AppendFormat("");
    CHECK(b.Empty());
  }

  SUBCASE("Single { not followed by } becomes literal { (and continues)") {
    StringBuilder<> b;
    b.ReserveFor(200);
    b.AppendFormat("a{b{}c", 7);
    // Walk it:
    // finds '{' after 'a' -> appends 'a', drops to "{b{}c"
    // drops '{', next is 'b' != '}' => appends '{' literally, recurse with same arg on "b{}c"
    // finds '{' before '{}' -> appends 'b', drops, sees '}' => appends arg(7), then "c"
    CHECK(b.ToSlice() == StringSlice("a{b7c"));
  }

  SUBCASE("Consecutive placeholders") {
    StringBuilder<> b;
    b.ReserveFor(200);
    b.AppendFormat("{}{}{}", 1, 2, 3);
    CHECK(b.ToSlice() == StringSlice("123"));
  }

  SUBCASE("Text after last placeholder") {
    StringBuilder<> b;
    b.ReserveFor(200);
    b.AppendFormat("v={}.", 9);
    CHECK(b.ToSlice() == StringSlice("v=9."));
  }
}

TEST_CASE("Appendf equivalence: Appendf vs manual Append") {
  StringBuilder<> a;
  StringBuilder<> b;

  a.AppendFormat("x{}y{}z", 10, StringSlice("QQ"));

  b.Append(StringSlice("x"));
  b.Append(10);
  b.Append("y");
  b.Append(StringSlice("QQ"));
  b.Append("z");

  CHECK(a.ToSlice() == b.ToSlice());
}

TEST_CASE("Multiple appends grow content correctly") {
  StringBuilder<> b;
  b.ReserveFor(200);

  for (int i = 0; i < 50; ++i) b.Append('a');
  CHECK(b.Length() == 50);

  auto s = b.ToSlice();
  CHECK(s.Size() == 50);
  for (size_t i = 0; i < s.Size(); ++i) CHECK(s[i] == 'a');
}

TEST_CASE("StringBuilder grows beyond stack buffer") {
  pdp::StringBuilder<> sb;

  // Push more than the internal 256-byte stack buffer
  for (int i = 0; i < 1000; ++i) {
    sb.Append('x');
  }

  CHECK(sb.Size() == 1000);

  // Verify contents
  for (size_t i = 0; i < sb.Size(); ++i) {
    CHECK(sb.Begin()[i] == 'x');
  }
}

TEST_CASE("AppendUninitialized appends raw space and preserves existing data") {
  StringBuilder<> b;
  b.ReserveFor(64);

  // Seed with known content
  b.Append("hello ");

  // Append raw space
  char *p = b.AppendUninitialized(5);

  // Finish off.
  b.Append("!!!");

  // Must point exactly to the old end
  REQUIRE(p != nullptr);

  // Fill manually
  p[0] = 'w';
  p[1] = 'o';
  p[2] = 'r';
  p[3] = 'l';
  p[4] = 'd';

  CHECK(b.ToSlice() == StringSlice("hello world!!!"));
}

TEST_CASE("StringBuilder::Truncate") {
  StringBuilder<> b;

  b.Append("abcdef");
  REQUIRE(b.ToSlice() == StringSlice("abcdef"));

  b.Truncate(3);
  CHECK(b.ToSlice() == StringSlice("abc"));

  b.Append("XYZ");
  CHECK(b.ToSlice() == StringSlice("abcXYZ"));

  b.Truncate(0);
  CHECK(b.ToSlice().Size() == 0);
}

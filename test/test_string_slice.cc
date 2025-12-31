#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "strings/string_slice.h"

using pdp::StringSlice;

TEST_CASE("StringSlice constructors") {
  SUBCASE("const char* constructor") {
    StringSlice s("hello");
    CHECK(s.Size() == 5);
    CHECK(s[0] == 'h');
    CHECK(s[1] == 'e');
    CHECK(s[2] == 'l');
    CHECK(s[3] == 'l');
    CHECK(s[4] == 'o');
  }

  SUBCASE("pointer + size constructor") {
    const char *p = "abcdef";
    StringSlice s(p + 1, 3);
    CHECK(s.Size() == 3);
    CHECK(s[0] == 'b');
    CHECK(s[2] == 'd');
  }

  SUBCASE("begin/end constructor") {
    const char *p = "abcdef";
    StringSlice s(p + 2, p + 5);
    CHECK(s.Size() == 3);
    CHECK(s[0] == 'c');
    CHECK(s[2] == 'e');
  }

  SUBCASE("empty string") {
    StringSlice s("");
    CHECK(s.Empty());
    CHECK(s.Size() == 0);
    CHECK(s.Begin() == s.End());
  }
}

TEST_CASE("Data / Begin / End") {
  StringSlice s("hello");
  CHECK(s.Data() == s.Begin());
  CHECK(s.End() == s.Begin() + 5);
}

TEST_CASE("Find(char)") {
  StringSlice s("abcabc");

  CHECK(*s.Find('a') == 'a');
  CHECK(*s.Find('b') == 'b');
  CHECK(*s.Find('c') == 'c');

  const char *not_found = s.Find('x');
  CHECK(not_found == s.End());
}

TEST_CASE("Find(it, char)") {
  StringSlice s("abcabc");

  const char *start = s.Begin() + 3;
  const char *p = s.Find(start, 'a');
  CHECK(p == s.Begin() + 3);

  const char *none = s.Find(start, 'z');
  CHECK(none == s.End());
}

TEST_CASE("MemReverseChar") {
  StringSlice s("abca");

  const char *p = s.MemReverseChar('a');
  CHECK(p != nullptr);
  CHECK(*p == 'a');
  CHECK(p == s.Begin() + 3);

  CHECK(s.MemReverseChar('z') == nullptr);
}

TEST_CASE("Substr(pos)") {
  StringSlice s("abcdef");

  auto sub = s.Substr(2);
  CHECK(sub.Size() == 4);
  CHECK(sub[0] == 'c');
  CHECK(sub[3] == 'f');
}

TEST_CASE("TakeLeft") {
  StringSlice s("abcdef");

  const char *it = s.Begin() + 3;
  auto left = s.TakeLeft(it);

  CHECK(left.Size() == 3);
  CHECK(left[0] == 'a');
  CHECK(left[2] == 'c');

  CHECK(s.Size() == 3);
  CHECK(s[0] == 'd');
  CHECK(s[2] == 'f');
}

TEST_CASE("DropLeft(pointer)") {
  StringSlice s("abcdef");

  const char *it = s.Begin() + 2;
  s.DropLeft(it);

  CHECK(s.Size() == 4);
  CHECK(s[0] == 'c');
  CHECK(s[3] == 'f');
}

TEST_CASE("DropLeft(size_t)") {
  StringSlice s("abcdef");

  s.DropLeft(4);
  CHECK(s.Size() == 2);
  CHECK(s[0] == 'e');
  CHECK(s[1] == 'f');
}

TEST_CASE("StartsWith") {
  StringSlice s("hello");
  CHECK(s.StartsWith('h'));
  CHECK_FALSE(s.StartsWith('x'));

  StringSlice empty("");
  CHECK_FALSE(empty.StartsWith('h'));
}

TEST_CASE("Empty / Size / Length") {
  StringSlice s("abc");
  CHECK_FALSE(s.Empty());
  CHECK(s.Size() == 3);
  CHECK(s.Length() == 3);

  StringSlice e("");
  CHECK(e.Empty());
  CHECK(e.Size() == 0);
}

TEST_CASE("operator==") {
  StringSlice a("hello");
  StringSlice b("hello");
  StringSlice c("world");
  StringSlice d("hell");

  CHECK(a == b);
  CHECK_FALSE(a == c);
  CHECK_FALSE(a == d);
}

TEST_CASE("operator!=") {
  StringSlice a("hello");
  StringSlice b("world");

  CHECK(a != b);
  CHECK_FALSE(a != StringSlice("hello"));
}

TEST_CASE("operator[]") {
  StringSlice s("xyz");

  CHECK(s[0] == 'x');
  CHECK(s[1] == 'y');
  CHECK(s[2] == 'z');
}

TEST_CASE("Chained mutation behavior") {
  StringSlice s("abcdef");

  auto a = s.TakeLeft(s.Begin() + 2);
  CHECK(a == StringSlice("ab"));

  s.DropLeft(1);
  CHECK(s == StringSlice("def"));
}

TEST_CASE("Find + TakeLeft integration") {
  StringSlice s("key=value");

  const char *eq = s.Find('=');
  auto key = s.TakeLeft(eq);

  CHECK(key == StringSlice("key"));
  CHECK(s.Size() == 6);
  CHECK(s[0] == '=');
}

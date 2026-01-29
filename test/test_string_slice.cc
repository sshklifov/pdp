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

TEST_CASE("MemReverseChar") {
  StringSlice s("abca");

  const char *p = s.MemReverseChar('a');
  CHECK(p != nullptr);
  CHECK(*p == 'a');
  CHECK(p == s.Begin() + 3);

  CHECK(s.MemReverseChar('z') == nullptr);
}

TEST_CASE("MemChar") {
  StringSlice s("abcac");

  const char *p = s.MemChar('c');
  CHECK(p != nullptr);
  CHECK(*p == 'c');
  CHECK(p == s.Begin() + 2);

  CHECK(s.MemChar('z') == nullptr);
}

TEST_CASE("Substr(pos)") {
  StringSlice s("abcdef");

  auto sub = s.Substr(s.Begin() + 2);
  CHECK(sub.Size() == 4);
  CHECK(sub[0] == 'c');
  CHECK(sub[3] == 'f');
}

TEST_CASE("StringSlice::GetLeft(size_t)") {
  StringSlice s("abcdef");

  SUBCASE("zero length") {
    auto left = s.GetLeft(0ul);
    CHECK(left.Size() == 0);
    CHECK(left.Begin() == s.Begin());
  }

  SUBCASE("partial") {
    auto left = s.GetLeft(3);
    CHECK(left.Size() == 3);
    CHECK(left == StringSlice("abc"));
  }

  SUBCASE("full size") {
    auto left = s.GetLeft(s.Size());
    CHECK(left.Size() == s.Size());
    CHECK(left == s);
  }
}

TEST_CASE("StringSlice::GetLeft(const char*)") {
  StringSlice s("abcdef");

  SUBCASE("begin iterator") {
    auto left = s.GetLeft(s.Begin());
    CHECK(left.Size() == 0);
    CHECK(left.Begin() == s.Begin());
  }

  SUBCASE("middle iterator") {
    const char *it = s.Begin() + 4;
    auto left = s.GetLeft(it);
    CHECK(left.Size() == 4);
    CHECK(left == StringSlice("abcd"));
  }

  SUBCASE("end iterator") {
    auto left = s.GetLeft(s.End());
    CHECK(left.Size() == s.Size());
    CHECK(left == s);
  }
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

TEST_CASE("EndsWith") {
  StringSlice s("hello there");
  CHECK(s.EndsWith("ere"));
  CHECK(s.EndsWith("o there"));
  CHECK_FALSE(s.EndsWith("rere"));

  StringSlice empty("");
  CHECK_FALSE(empty.EndsWith("there"));
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

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "strings/fixed_string.h"
#include "strings/string_slice.h"

using namespace pdp;

TEST_CASE("DynamicString default construction") {
  FixedString s;

  CHECK(s.Cstr() == nullptr);
  CHECK(s.Size() == 0);
  CHECK(s.Empty());
}

TEST_CASE("DynamicString construct from pointer + length") {
  const char *txt = "hello";
  FixedString s(txt, 5);

  CHECK(s.Size() == 5);
  CHECK(!s.Empty());
  CHECK(s[0] == 'h');
  CHECK(s[4] == 'o');
  CHECK(s.Cstr()[5] == '\0');  // null termination is REQUIRED
}

TEST_CASE("DynamicString construct from begin/end") {
  const char buf[] = "abcdef";
  FixedString s(buf + 1, buf + 4);  // "bcd"

  CHECK(s.Size() == 3);
  CHECK(s == StringSlice("bcd", 3));
}

TEST_CASE("DynamicString move constructor transfers ownership") {
  FixedString a("move", 4);
  const char *old_ptr = a.Cstr();

  FixedString b(std::move(a));

  CHECK(b.Size() == 4);
  CHECK(b.Cstr() == old_ptr);
  CHECK(a.Cstr() == nullptr);  // must be null to avoid double free
}

TEST_CASE("DynamicString equality with DynamicString") {
  FixedString a("test", 4);
  FixedString b("test", 4);
  FixedString c("diff", 4);

  CHECK(a == b);
  CHECK(a != c);
}

TEST_CASE("DynamicString equality with StringSlice") {
  FixedString s("slice", 5);
  StringSlice slice("slice", 5);
  StringSlice other("other", 5);

  CHECK(s == slice);
  CHECK(s != other);
}

TEST_CASE("DynamicString GetSlice returns correct view") {
  FixedString s("view", 4);
  StringSlice slice = s.ToSlice();

  CHECK(slice.Data() == s.Cstr());
  CHECK(slice.Size() == 4);
  CHECK(s == slice);
}

TEST_CASE("DynamicString Data, Begin, End iterators behave correctly") {
  FixedString s("abcd", 4);

  // Data() and Begin() must be identical
  CHECK(s.Cstr() == s.Begin());

  const char *it = s.Begin();
  const char *end = s.End();

  REQUIRE(end - it == 4);

  CHECK(*it++ == 'a');
  CHECK(*it++ == 'b');
  CHECK(*it++ == 'c');
  CHECK(*it++ == 'd');

  CHECK(it == end);
}

TEST_CASE("DynamicString Begin == End for empty string") {
  FixedString s;

  CHECK(s.Begin() == s.End());
  CHECK(s.Cstr() == nullptr);
}

TEST_CASE("DynamicString Size, Length, Empty are consistent") {
  FixedString empty;
  CHECK(empty.Size() == 0);
  CHECK(empty.Length() == 0);
  CHECK(empty.Empty());

  FixedString s("xyz", 3);
  CHECK(s.Size() == 3);
  CHECK(s.Length() == 3);
  CHECK(!s.Empty());
}

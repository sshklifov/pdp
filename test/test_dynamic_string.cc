#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "strings/dynamic_string.h"
#include "strings/string_slice.h"

using namespace pdp;

TEST_CASE("DynamicString default construction") {
  DynamicString s;

  CHECK(s.Data() == nullptr);
  CHECK(s.Size() == 0);
  CHECK(s.Empty());
}

TEST_CASE("DynamicString construct from pointer + length") {
  const char *txt = "hello";
  DynamicString s(txt, 5);

  CHECK(s.Size() == 5);
  CHECK(!s.Empty());
  CHECK(s[0] == 'h');
  CHECK(s[4] == 'o');
  CHECK(s.Data()[5] == '\0');  // null termination is REQUIRED
}

TEST_CASE("DynamicString construct from begin/end") {
  const char buf[] = "abcdef";
  DynamicString s(buf + 1, buf + 4);  // "bcd"

  CHECK(s.Size() == 3);
  CHECK(s == StringSlice("bcd", 3));
}

TEST_CASE("DynamicString move constructor transfers ownership") {
  DynamicString a("move", 4);
  const char *old_ptr = a.Data();

  DynamicString b(std::move(a));

  CHECK(b.Size() == 4);
  CHECK(b.Data() == old_ptr);
  CHECK(a.Data() == nullptr);  // must be null to avoid double free
}

TEST_CASE("DynamicString equality with DynamicString") {
  DynamicString a("test", 4);
  DynamicString b("test", 4);
  DynamicString c("diff", 4);

  CHECK(a == b);
  CHECK(a != c);
}

TEST_CASE("DynamicString equality with StringSlice") {
  DynamicString s("slice", 5);
  StringSlice slice("slice", 5);
  StringSlice other("other", 5);

  CHECK(s == slice);
  CHECK(s != other);
}

TEST_CASE("DynamicString GetSlice returns correct view") {
  DynamicString s("view", 4);
  StringSlice slice = s.ToSlice();

  CHECK(slice.Data() == s.Data());
  CHECK(slice.Size() == 4);
  CHECK(s == slice);
}

TEST_CASE("InPlaceStringInit initializes empty DynamicString") {
  DynamicString s;
  impl::_DynamicStringPrivInit init(s);

  char *buf = init(6);
  memcpy(buf, "foobar", 6);

  CHECK(s.Size() == 6);
  CHECK(s == StringSlice("foobar", 6));
  CHECK(s.Data()[6] == '\0');
}

TEST_CASE("DynamicString Data, Begin, End iterators behave correctly") {
  DynamicString s("abcd", 4);

  // Data() and Begin() must be identical
  CHECK(s.Data() == s.Begin());

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
  DynamicString s;

  CHECK(s.Begin() == s.End());
  CHECK(s.Data() == nullptr);
}

TEST_CASE("DynamicString Size, Length, Empty are consistent") {
  DynamicString empty;
  CHECK(empty.Size() == 0);
  CHECK(empty.Length() == 0);
  CHECK(empty.Empty());

  DynamicString s("xyz", 3);
  CHECK(s.Size() == 3);
  CHECK(s.Length() == 3);
  CHECK(!s.Empty());
}

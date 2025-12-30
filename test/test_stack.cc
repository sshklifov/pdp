#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "data/stack.h"

using pdp::Stack;

/* ===============================
   Basic stack behavior
   =============================== */

TEST_CASE("Stack push/pop follows LIFO order") {
  Stack<uint32_t> s(4);

  s.Push(1);
  s.Push(2);
  s.Push(3);

  CHECK(s.Size() == 3);
  CHECK(s.Last() == 3);

  s.Pop();
  CHECK(s.Size() == 2);
  CHECK(s.Last() == 2);

  s.Pop();
  CHECK(s.Size() == 1);
  CHECK(s.Last() == 1);

  s.Pop();
  CHECK(s.Empty());
}

/* ===============================
   Pop does not reallocate
   =============================== */

TEST_CASE("Stack Pop does not change capacity or allocation") {
  Stack<uint32_t> s(4);

  s.Push(10);
  s.Push(20);
  s.Push(30);

  auto cap_before = s.Capacity();

  s.Pop();
  s.Pop();

  CHECK(s.Capacity() == cap_before);
}

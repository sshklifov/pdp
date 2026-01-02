#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "parser/mi_expr.h"
#include "parser/mi_parser.h"
#include "strings/string_slice.h"

using namespace pdp;

TEST_CASE("simple param tuples") {
  {
    StringSlice input("param=\"pagination\",value=\"off\"");
    MiFirstPass first(input);
    REQUIRE(first.Parse());

    MISecondPass second(input, first);
    MiNiceExpr e(second.Parse());

    CHECK(e.Count() == 2);
    CHECK(e["param"].StringOr("X") == "pagination");
    CHECK(e["value"].StringOr("X") == "off");
  }

  {
    StringSlice input("param=\"inferior-tty\",value=\"/dev/pts/0\"");
    MiFirstPass first(input);
    REQUIRE(first.Parse());

    MISecondPass second(input, first);
    MiNiceExpr e(second.Parse());

    CHECK(e["param"].StringOr("X") == "inferior-tty");
    CHECK(e["value"].StringOr("X") == "/dev/pts/0");
  }

  {
    StringSlice input("param=\"prompt\",value=\"\"");
    MiFirstPass first(input);
    REQUIRE(first.Parse());

    MISecondPass second(input, first);
    MiNiceExpr e(second.Parse());

    CHECK(e["param"].StringOr("X") == "prompt");
    CHECK(e["value"].StringOr("NOT_EMPTY") == "");
  }

  {
    StringSlice input("param=\"max-completions\",value=\"20\"");
    MiFirstPass first(input);
    REQUIRE(first.Parse());

    MISecondPass second(input, first);
    MiNiceExpr e(second.Parse());

    CHECK(e["param"].StringOr("X") == "max-completions");
    CHECK(e["value"].NumberOr(-1) == 20);
  }

  {
    StringSlice input("param=\"startup-with-shell\",value=\"off\"");
    MiFirstPass first(input);
    REQUIRE(first.Parse());

    MISecondPass second(input, first);
    MiNiceExpr e(second.Parse());

    CHECK(e["param"].StringOr("X") == "startup-with-shell");
    CHECK(e["value"].StringOr("X") == "off");
  }
}

TEST_CASE("bkpt tuple with mixed fields") {
  StringSlice input(
      "bkpt={"
      "number=\"1\",type=\"breakpoint\",disp=\"del\",enabled=\"y\",addr=\"0x00000000000039fc\","
      "func=\"main(int, char**)\",file=\"/home/stef/backtrace_tool.cc\","
      "fullname=\"/home/stef/backtrace_tool.cc\",line=\"182\",thread-groups=[\"i1\"],times=\"0\","
      "original-location=\"-qualified main\""
      "}");

  MiFirstPass first(input);
  REQUIRE(first.Parse());

  MISecondPass second(input, first);
  MiNiceExpr e(second.Parse());

  auto bkpt = e["bkpt"];
  CHECK(bkpt.Count() > 5);

  CHECK(bkpt["number"].NumberOr(-1) == 1);
  CHECK(bkpt["type"].StringOr("X") == "breakpoint");
  CHECK(bkpt["disp"].StringOr("X") == "del");
  CHECK(bkpt["enabled"].StringOr("X") == "y");
  CHECK(bkpt["addr"].StringOr("X") == "0x00000000000039fc");
  CHECK(bkpt["line"].NumberOr(-1) == 182);

  auto tg = bkpt["thread-groups"];
  CHECK(tg.Count() == 1);
  CHECK(tg[0].StringOr("X") == "i1");
}

TEST_CASE("shared object with ranges list") {
  StringSlice input(
      "id=\"/lib/ld-linux-aarch64.so.1\",target-name=\"/lib/ld-linux-aarch64.so.1\","
      "host-name=\"/lib/ld-linux-aarch64.so.1\",symbols-loaded=\"0\",thread-group=\"i1\","
      "ranges=[{from=\"0x0000007ff7fc3d80\",to=\"0x0000007ff7fe1328\"}]");

  MiFirstPass first(input);
  REQUIRE(first.Parse());

  MISecondPass second(input, first);
  MiNiceExpr e(second.Parse());

  CHECK(e["id"].StringOr("X") == "/lib/ld-linux-aarch64.so.1");
  CHECK(e["symbols-loaded"].NumberOr(-1) == 0);

  auto ranges = e["ranges"];
  CHECK(ranges.Count() == 1);

  auto r0 = ranges[0];
  CHECK(r0.Count() == 2);
  CHECK(r0["from"].StringOr("X") == "0x0000007ff7fc3d80");
  CHECK(r0["to"].StringOr("X") == "0x0000007ff7fe1328");
}

TEST_CASE("stop reason with nested frame and args") {
  StringSlice input(
      "reason=\"breakpoint-hit\",disp=\"del\",bkptno=\"1\",frame={addr=\"0x00000055555539fc\","
      "func=\"main\",args=[{name=\"argc\"},{name=\"argv\"}],file=\"/home/stef/backtrace_tool.cc\","
      "fullname=\"/home/stef/backtrace_tool.cc\",line=\"182\",arch=\"aarch64\"},thread-id=\"1\","
      "stopped-threads=\"all\",core=\"2\"");

  MiFirstPass first(input);
  REQUIRE(first.Parse());

  MISecondPass second(input, first);
  MiNiceExpr e(second.Parse());

  CHECK(e["reason"].StringOr("X") == "breakpoint-hit");
  CHECK(e["bkptno"].NumberOr(-1) == 1);
  CHECK(e["core"].NumberOr(-1) == 2);

  auto frame = e["frame"];
  CHECK(frame["func"].StringOr("X") == "main");
  CHECK(frame["line"].NumberOr(-1) == 182);
  CHECK(frame["arch"].StringOr("X") == "aarch64");

  auto args = frame["args"];
  CHECK(args.Count() == 2);
  CHECK(args[0]["name"].StringOr("X") == "argc");
  CHECK(args[1]["name"].StringOr("X") == "argv");
}

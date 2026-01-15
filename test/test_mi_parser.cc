#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "parser/expr.h"
#include "parser/mi_parser.h"
#include "strings/string_slice.h"

using namespace pdp;

TEST_CASE("simple param tuples") {
  {
    StringSlice input("param=\"pagination\",value=\"off\"");
    MiFirstPass first(input);
    REQUIRE(first.Parse());

    MiSecondPass second(input, first);
    auto ptr = second.Parse();
    LooseTypedView e(ptr.Get());

    CHECK(e.Count() == 2);
    CHECK(e["param"].AsString() == "pagination");
    CHECK(e["value"].AsString() == "off");
  }

  {
    StringSlice input("param=\"inferior-tty\",value=\"/dev/pts/0\"");
    MiFirstPass first(input);
    REQUIRE(first.Parse());

    MiSecondPass second(input, first);
    auto ptr = second.Parse();
    LooseTypedView e(ptr.Get());

    CHECK(e["param"].AsString() == "inferior-tty");
    CHECK(e["value"].AsString() == "/dev/pts/0");
  }

  {
    StringSlice input("param=\"prompt\",value=\"\"");
    MiFirstPass first(input);
    REQUIRE(first.Parse());

    MiSecondPass second(input, first);
    auto ptr = second.Parse();
    LooseTypedView e(ptr.Get());

    CHECK(e["param"].AsString() == "prompt");
    CHECK(e["value"].AsString() == "");
  }

  {
    StringSlice input("param=\"max-completions\",value=\"20\"");
    MiFirstPass first(input);
    REQUIRE(first.Parse());

    MiSecondPass second(input, first);
    auto ptr = second.Parse();
    LooseTypedView e(ptr.Get());

    CHECK(e["param"].AsString() == "max-completions");
    CHECK(e["value"].AsInteger() == 20);
  }

  {
    StringSlice input("param=\"startup-with-shell\",value=\"off\"");
    MiFirstPass first(input);
    REQUIRE(first.Parse());

    MiSecondPass second(input, first);
    auto ptr = second.Parse();
    LooseTypedView e(ptr.Get());

    CHECK(e["param"].AsString() == "startup-with-shell");
    CHECK(e["value"].AsString() == "off");
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

  MiSecondPass second(input, first);
  auto ptr = second.Parse();
  LooseTypedView e(ptr.Get());

  auto bkpt = e["bkpt"];
  CHECK(bkpt.Count() > 5);

  CHECK(bkpt["number"].AsInteger() == 1);
  CHECK(bkpt["type"].AsString() == "breakpoint");
  CHECK(bkpt["disp"].AsString() == "del");
  CHECK(bkpt["enabled"].AsString() == "y");
  CHECK(bkpt["addr"].AsString() == "0x00000000000039fc");
  CHECK(bkpt["line"].AsInteger() == 182);

  auto tg = bkpt["thread-groups"];
  CHECK(tg.Count() == 1);
  CHECK(tg[0u].AsString() == "i1");
}

TEST_CASE("shared object with ranges list") {
  StringSlice input(
      "id=\"/lib/ld-linux-aarch64.so.1\",target-name=\"/lib/ld-linux-aarch64.so.1\","
      "host-name=\"/lib/ld-linux-aarch64.so.1\",symbols-loaded=\"0\",thread-group=\"i1\","
      "ranges=[{from=\"0x0000007ff7fc3d80\",to=\"0x0000007ff7fe1328\"}]");

  MiFirstPass first(input);
  REQUIRE(first.Parse());

  MiSecondPass second(input, first);
  auto ptr = second.Parse();
  LooseTypedView e(ptr.Get());

  CHECK(e["id"].AsString() == "/lib/ld-linux-aarch64.so.1");
  CHECK(e["symbols-loaded"].AsInteger() == 0);

  auto ranges = e["ranges"];
  CHECK(ranges.Count() == 1);

  auto r0 = ranges[0u];
  CHECK(r0.Count() == 2);
  CHECK(r0["from"].AsString() == "0x0000007ff7fc3d80");
  CHECK(r0["to"].AsString() == "0x0000007ff7fe1328");
}

TEST_CASE("stop reason with nested frame and args") {
  StringSlice input(
      "reason=\"breakpoint-hit\",disp=\"del\",bkptno=\"1\",frame={addr=\"0x00000055555539fc\","
      "func=\"main\",args=[{name=\"argc\"},{name=\"argv\"}],file=\"/home/stef/backtrace_tool.cc\","
      "fullname=\"/home/stef/backtrace_tool.cc\",line=\"182\",arch=\"aarch64\"},thread-id=\"1\","
      "stopped-threads=\"all\",core=\"2\"");

  MiFirstPass first(input);
  REQUIRE(first.Parse());

  MiSecondPass second(input, first);
  auto ptr = second.Parse();
  LooseTypedView e(ptr.Get());

  CHECK(e["reason"].AsString() == "breakpoint-hit");
  CHECK(e["bkptno"].AsInteger() == 1);
  CHECK(e["core"].AsInteger() == 2);

  auto frame = e["frame"];
  CHECK(frame["func"].AsString() == "main");
  CHECK(frame["line"].AsInteger() == 182);
  CHECK(frame["arch"].AsString() == "aarch64");

  auto args = frame["args"];
  CHECK(args.Count() == 2);
  CHECK(args[0u]["name"].AsString() == "argc");
  CHECK(args[1u]["name"].AsString() == "argv");
}

#include <sys/wait.h>
#include <thread>
#include "data/scoped_ptr.h"
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "parser/expr.h"
#include "parser/rpc_builder.h"
#include "parser/rpc_parser.h"

using namespace pdp;

static void WriteAll(int fd, const void *buf, size_t size) {
  const char *p = static_cast<const char *>(buf);
  while (size > 0) {
    ssize_t n = write(fd, p, size);
    REQUIRE(n > 0);
    p += n;
    size -= n;
  }
}

struct ParseResult {
  ExprView expr;
  ChunkHandle chunks;
};

static ParseResult ParseFromBuilder(const StringSlice &msg) {
  int fds[2];
  REQUIRE(pipe(fds) == 0);

  WriteAll(fds[1], msg.Data(), msg.Size());
  close(fds[1]);

  RpcChunkArrayPass pass(fds[0]);
  ExprView res = pass.Parse();
  auto chunks = pass.ReleaseChunks();
  return {res, std::move(chunks)};
}

static ScopedPtr<char> MakeString(size_t len, char c = 'x') {
  ScopedPtr<char> res((char *)malloc(len));
  memset(res.Get(), c, len);
  return res;
}

TEST_CASE("rpc builder: notification with no args") {
  RpcBuilder b(2, "ping");
  {
    auto empty_args = b.AddArray();
  }
  StringSlice msg = b.Finish();

  auto [e, chunks] = ParseFromBuilder(msg);

  REQUIRE(e.Count() == 4);
  CHECK(e[0].NumberOr(-1) == 0);
  CHECK(e[1].NumberOr(-1) == 2);
  CHECK(e[2].StringOr("X") == "ping");
  CHECK(e[3].Count() == 0);
}

TEST_CASE("rpc builder: notification scalar arguments") {
  RpcBuilder b(2, "set_config");
  {
    auto args = b.AddArray();
    b.AddInteger(-42);
    b.AddUnsigned(123u);
    b.AddBoolean(true);
    b.AddBoolean(false);
    b.AddString("hello");
  }

  auto [e, chunks] = ParseFromBuilder(b.Finish());

  REQUIRE(e[3].Count() == 5);
  CHECK(e[3][0].NumberOr(0) == -42);
  CHECK(e[3][1].NumberOr(0) == 123);
  CHECK(e[3][2].NumberOr(-1) == true);
  CHECK(e[3][3].NumberOr(-1) == false);
  CHECK(e[3][4].StringOr("X") == "hello");
}

TEST_CASE("rpc builder: nested arrays in args") {
  RpcBuilder b(2, "nested_test");
  {
    auto args = b.AddArray();
    b.AddInteger(1);

    {
      auto inner = b.AddArray();
      b.AddInteger(2);
      b.AddInteger(3);
    }

    b.AddInteger(4);
  }

  auto [e, chunks] = ParseFromBuilder(b.Finish());

  REQUIRE(e[3].Count() == 3);
  CHECK(e[3][0].NumberOr(0) == 1);

  REQUIRE(e[3][1].Count() == 2);
  CHECK(e[3][1][0].NumberOr(0) == 2);
  CHECK(e[3][1][1].NumberOr(0) == 3);

  CHECK(e[3][2].NumberOr(0) == 4);
}

TEST_CASE("rpc builder: array element count correctness") {
  RpcBuilder b(2, "count_test");
  {
    auto args = b.AddArray();
    for (int i = 0; i < 10; ++i) {
      b.AddInteger(i);
    }
  }

  auto [e, chunks] = ParseFromBuilder(b.Finish());

  REQUIRE(e[3].Count() == 10);
  for (int i = 0; i < 10; ++i) {
    CHECK(e[3][i].NumberOr(-1) == i);
  }
}

TEST_CASE("rpc builder: nested arrays up to max depth") {
  RpcBuilder b(2, "depth_test");
  {
    auto a0 = b.AddArray();
    auto a1 = b.AddArray();
    auto a2 = b.AddArray();
    auto a3 = b.AddArray();
    auto a4 = b.AddArray();
    auto a5 = b.AddArray();
    auto a6 = b.AddArray();

    b.AddInteger(123);
  }

  auto [e, chunks] = ParseFromBuilder(b.Finish());

  ExprView cur = e[3];
  for (int i = 0; i < 6; ++i) {
    REQUIRE(cur.Count() == 1);
    cur = cur[0];
  }

  REQUIRE(cur.Count() == 1);
  CHECK(cur[0].NumberOr(0) == 123);
}

TEST_CASE("rpc builder: unsigned integer boundaries") {
  RpcBuilder b(2, "uint_test");
  {
    auto args = b.AddArray();
    b.AddUnsigned(0);
    b.AddUnsigned(127);          // max positive fixint
    b.AddUnsigned(128);          // uint8
    b.AddUnsigned(255);          // max uint8
    b.AddUnsigned(256);          // uint16
    b.AddUnsigned(65535);        // max uint16
    b.AddUnsigned(65536);        // uint32
    b.AddUnsigned(0xFFFFFFFFu);  // max uint32
  }

  auto [e, chunks] = ParseFromBuilder(b.Finish());
  REQUIRE(e[3].Count() == 8);

  CHECK(e[3][0].NumberOr(-1) == 0);
  CHECK(e[3][1].NumberOr(-1) == 127);
  CHECK(e[3][2].NumberOr(-1) == 128);
  CHECK(e[3][3].NumberOr(-1) == 255);
  CHECK(e[3][4].NumberOr(-1) == 256);
  CHECK(e[3][5].NumberOr(-1) == 65535);
  CHECK(e[3][6].NumberOr(-1) == 65536);
  CHECK(e[3][7].NumberOr(-1) == static_cast<int64_t>(0xFFFFFFFFu));
}

TEST_CASE("rpc builder: signed integer boundaries") {
  RpcBuilder b(2, "int_test");
  {
    auto args = b.AddArray();
    b.AddInteger(0);
    b.AddInteger(-1);         // fixneg
    b.AddInteger(-32);        // min fixneg
    b.AddInteger(-33);        // int8
    b.AddInteger(-128);       // min int8
    b.AddInteger(-129);       // int16
    b.AddInteger(-32768);     // min int16
    b.AddInteger(-32769);     // int32
    b.AddInteger(INT32_MIN);  // min int32
    b.AddInteger(INT32_MAX);  // max int32
  }

  auto [e, chunks] = ParseFromBuilder(b.Finish());
  REQUIRE(e[3].Count() == 10);

  CHECK(e[3][0].NumberOr(1) == 0);
  CHECK(e[3][1].NumberOr(0) == -1);
  CHECK(e[3][2].NumberOr(0) == -32);
  CHECK(e[3][3].NumberOr(0) == -33);
  CHECK(e[3][4].NumberOr(0) == -128);
  CHECK(e[3][5].NumberOr(0) == -129);
  CHECK(e[3][6].NumberOr(0) == -32768);
  CHECK(e[3][7].NumberOr(0) == -32769);
  CHECK(e[3][8].NumberOr(0) == INT32_MIN);
  CHECK(e[3][9].NumberOr(0) == INT32_MAX);
}

TEST_CASE("rpc builder: fixstr boundaries") {
  auto ptr1 = MakeString(30);
  StringSlice str1(ptr1.Get(), 30);
  auto ptr2 = MakeString(31);
  StringSlice str2(ptr2.Get(), 30);

  RpcBuilder b(2, "fixstr_test");
  {
    auto args = b.AddArray();
    b.AddString("");    // 0
    b.AddString("a");   // 1
    b.AddString(str1);  // 30
    b.AddString(str2);  // 31
  }

  auto [e, chunks] = ParseFromBuilder(b.Finish());
  REQUIRE(e[3].Count() == 4);

  CHECK(e[3][0].StringOr("X") == "");
  CHECK(e[3][1].StringOr("X") == "a");
  CHECK(e[3][2].StringOr("X") == str1);
  CHECK(e[3][3].StringOr("X") == str2);
}

TEST_CASE("rpc builder: str8 boundaries") {
  auto ptr31 = MakeString(31);
  StringSlice str31(ptr31.Get(), 31);

  auto ptr32 = MakeString(32);
  StringSlice str32(ptr32.Get(), 32);

  auto ptr255 = MakeString(255);
  StringSlice str255(ptr255.Get(), 255);

  RpcBuilder b(2, "str8_test");
  {
    auto args = b.AddArray();
    b.AddString(str31);   // max fixstr
    b.AddString(str32);   // min str8
    b.AddString(str255);  // max str8
  }

  auto [e, chunks] = ParseFromBuilder(b.Finish());
  REQUIRE(e[3].Count() == 3);

  CHECK(e[3][0].StringOr("X") == str31);
  CHECK(e[3][1].StringOr("X") == str32);
  CHECK(e[3][2].StringOr("X") == str255);
}

TEST_CASE("rpc builder: str16 lower boundary") {
  auto ptr255 = MakeString(255);
  StringSlice str255(ptr255.Get(), 255);

  auto ptr256 = MakeString(256);
  StringSlice str256(ptr256.Get(), 256);

  RpcBuilder b(2, "str16_low_test");
  {
    auto args = b.AddArray();
    b.AddString(str255);  // str8
    b.AddString(str256);  // str16
  }

  auto [e, chunks] = ParseFromBuilder(b.Finish());
  REQUIRE(e[3].Count() == 2);

  CHECK(e[3][0].StringOr("X") == str255);
  CHECK(e[3][1].StringOr("X") == str256);
}

TEST_CASE("rpc builder: str16 range") {
  auto ptr256 = MakeString(256);
  StringSlice str256(ptr256.Get(), 256);

  auto ptr1024 = MakeString(1024);
  StringSlice str1024(ptr1024.Get(), 1024);

  auto ptr65535 = MakeString(65535);
  StringSlice str65535(ptr65535.Get(), 65535);

  RpcBuilder b(2, "str16_test");
  {
    auto args = b.AddArray();
    b.AddString(str256);
    b.AddString(str1024);
    b.AddString(str65535);
  }

  int fds[2];
  REQUIRE(pipe(fds) == 0);

  std::thread t([&]() {
    auto msg = b.Finish();
    WriteAll(fds[1], msg.Data(), msg.Size());
    close(fds[1]);
  });

  RpcChunkArrayPass pass(fds[0]);
  ExprView e = pass.Parse();
  auto chunks = pass.ReleaseChunks();

  REQUIRE(e[3].Count() == 3);

  CHECK(e[3][0].StringOr("X") == str256);
  CHECK(e[3][1].StringOr("X") == str1024);
  CHECK(e[3][2].StringOr("X") == str65535);

  t.join();
}

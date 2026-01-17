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
  StrongTypedView expr;
  ChunkHandle chunks;
};

static ParseResult ParseFromBuilder(const RpcBytes &msg) {
  int fds[2];
  REQUIRE(pipe(fds) == 0);

  WriteAll(fds[1], msg.data, msg.bytes);
  close(fds[1]);

  ByteStream stream(fds[0]);
  RpcChunkArrayPass pass(stream);
  StrongTypedView res = pass.Parse();
  auto chunks = pass.ReleaseChunks();
  return {res, std::move(chunks)};
}

static ScopedPtr<char> MakeString(size_t len, char c = 'x') {
  ScopedPtr<char> res((char *)malloc(len));
  memset(res.Get(), c, len);
  return res;
}

TEST_CASE("Appending bytes") {
  SUBCASE("AppendUnchecked") {
    ByteBuilder<DefaultAllocator> b;
    b.ReserveFor(10);
    unsigned char de = 0xde;
    b.AppendByteUnchecked(de);
    CHECK(b.Size() == 1);
    CHECK(memcmp(b.Data(), &de, 1) == 0);
  }
  SUBCASE("Normal Append") {
    ByteBuilder<DefaultAllocator> b;
    unsigned char bc = 0xbc;
    b.AppendByte(bc);
    CHECK(b.Size() == 1);
    CHECK(memcmp(b.Data(), &bc, 1) == 0);
  }
}

TEST_CASE("rpc builder: notification with no args") {
  RpcBuilder b(2, "ping");
  b.OpenShortArray();
  b.CloseShortArray();

  auto [e, chunks] = ParseFromBuilder(b.Finish());

  REQUIRE(e.Count() == 4);
  CHECK(e[0u].AsInteger() == 0);
  CHECK(e[1].AsInteger() == 2);
  CHECK(e[2].AsString() == "ping");
  CHECK(e[3].Count() == 0);
}

TEST_CASE("rpc builder: notification scalar arguments") {
  RpcBuilder b(2, "set_config");
  b.OpenShortArray();
  b.Add(-42);
  b.Add(123u);
  b.Add(true);
  b.Add(false);
  b.Add(StringSlice("hello"));
  b.CloseShortArray();

  auto [e, chunks] = ParseFromBuilder(b.Finish());

  REQUIRE(e[3].Count() == 5);
  CHECK(e[3][0u].AsInteger() == -42);
  CHECK(e[3][1].AsInteger() == 123);
  CHECK(e[3][2].AsInteger() == true);
  CHECK(e[3][3].AsInteger() == false);
  CHECK(e[3][4].AsString() == "hello");
}

TEST_CASE("rpc builder: nested arrays in args") {
  RpcBuilder b(2, "nested_test");
  b.OpenShortArray();
  b.Add(1);

  b.OpenShortArray();
  b.Add(2);
  b.Add(3);
  b.CloseShortArray();

  b.Add(4);
  b.CloseShortArray();

  auto [e, chunks] = ParseFromBuilder(b.Finish());

  REQUIRE(e[3].Count() == 3);
  CHECK(e[3][0u].AsInteger() == 1);

  REQUIRE(e[3][1].Count() == 2);
  CHECK(e[3][1][0u].AsInteger() == 2);
  CHECK(e[3][1][1].AsInteger() == 3);

  CHECK(e[3][2].AsInteger() == 4);
}

TEST_CASE("rpc builder: array element count correctness") {
  RpcBuilder b(2, "count_test");
  b.OpenShortArray();
  for (int i = 0; i < 10; ++i) {
    b.Add(i);
  }
  b.CloseShortArray();

  auto [e, chunks] = ParseFromBuilder(b.Finish());

  REQUIRE(e[3].Count() == 10);
  for (int i = 0; i < 10; ++i) {
    CHECK(e[3][i].AsInteger() == i);
  }
}

TEST_CASE("rpc builder: nested arrays up to max depth") {
  RpcBuilder b(2, "depth_test");
  b.OpenShortArray();
  b.OpenShortArray();
  b.OpenShortArray();
  b.OpenShortArray();
  b.OpenShortArray();
  b.OpenShortArray();
  b.OpenShortArray();

  b.Add(123);

  b.CloseShortArray();
  b.CloseShortArray();
  b.CloseShortArray();
  b.CloseShortArray();
  b.CloseShortArray();
  b.CloseShortArray();
  b.CloseShortArray();

  auto [e, chunks] = ParseFromBuilder(b.Finish());

  StrongTypedView cur = e[3];
  for (int i = 0; i < 6; ++i) {
    REQUIRE(cur.Count() == 1);
    cur = cur[0u];
  }

  REQUIRE(cur.Count() == 1);
  CHECK(cur[0u].AsInteger() == 123);
}

TEST_CASE("rpc builder: unsigned integer boundaries") {
  RpcBuilder b(2, "uint_test");
  b.OpenShortArray();
  b.Add(0);
  b.Add(127);          // max positive fixint
  b.Add(128);          // uint8
  b.Add(255);          // max uint8
  b.Add(256);          // uint16
  b.Add(65535);        // max uint16
  b.Add(65536);        // uint32
  b.Add(0xFFFFFFFFu);  // max uint32
  b.CloseShortArray();

  auto [e, chunks] = ParseFromBuilder(b.Finish());
  REQUIRE(e[3].Count() == 8);

  CHECK(e[3][0u].AsInteger() == 0);
  CHECK(e[3][1].AsInteger() == 127);
  CHECK(e[3][2].AsInteger() == 128);
  CHECK(e[3][3].AsInteger() == 255);
  CHECK(e[3][4].AsInteger() == 256);
  CHECK(e[3][5].AsInteger() == 65535);
  CHECK(e[3][6].AsInteger() == 65536);
  CHECK(e[3][7].AsInteger() == static_cast<int64_t>(0xFFFFFFFFu));
}

TEST_CASE("rpc builder: signed integer boundaries") {
  RpcBuilder b(2, "int_test");
  b.OpenShortArray();
  b.Add(0);
  b.Add(-1);         // fixneg
  b.Add(-32);        // min fixneg
  b.Add(-33);        // int8
  b.Add(-128);       // min int8
  b.Add(-129);       // int16
  b.Add(-32768);     // min int16
  b.Add(-32769);     // int32
  b.Add(INT32_MIN);  // min int32
  b.Add(INT32_MAX);  // max int32
  b.CloseShortArray();

  auto [e, chunks] = ParseFromBuilder(b.Finish());
  REQUIRE(e[3].Count() == 10);

  CHECK(e[3][0u].AsInteger() == 0);
  CHECK(e[3][1].AsInteger() == -1);
  CHECK(e[3][2].AsInteger() == -32);
  CHECK(e[3][3].AsInteger() == -33);
  CHECK(e[3][4].AsInteger() == -128);
  CHECK(e[3][5].AsInteger() == -129);
  CHECK(e[3][6].AsInteger() == -32768);
  CHECK(e[3][7].AsInteger() == -32769);
  CHECK(e[3][8].AsInteger() == INT32_MIN);
  CHECK(e[3][9].AsInteger() == INT32_MAX);
}

TEST_CASE("rpc builder: fixstr boundaries") {
  auto ptr1 = MakeString(30);
  StringSlice str1(ptr1.Get(), 30);
  auto ptr2 = MakeString(31);
  StringSlice str2(ptr2.Get(), 30);

  RpcBuilder b(2, "fixstr_test");
  b.OpenShortArray();
  b.Add(StringSlice(""));   // 0
  b.Add(StringSlice("a"));  // 1
  b.Add(str1);              // 30
  b.Add(str2);              // 31
  b.CloseShortArray();

  auto [e, chunks] = ParseFromBuilder(b.Finish());
  REQUIRE(e[3].Count() == 4);

  CHECK(e[3][0u].AsString() == "");
  CHECK(e[3][1].AsString() == "a");
  CHECK(e[3][2].AsString() == str1);
  CHECK(e[3][3].AsString() == str2);
}

TEST_CASE("rpc builder: str8 boundaries") {
  auto ptr31 = MakeString(31);
  StringSlice str31(ptr31.Get(), 31);

  auto ptr32 = MakeString(32);
  StringSlice str32(ptr32.Get(), 32);

  auto ptr255 = MakeString(255);
  StringSlice str255(ptr255.Get(), 255);

  RpcBuilder b(2, "str8_test");
  b.OpenShortArray();
  b.Add(str31);   // max fixstr
  b.Add(str32);   // min str8
  b.Add(str255);  // max str8
  b.CloseShortArray();

  auto [e, chunks] = ParseFromBuilder(b.Finish());
  REQUIRE(e[3].Count() == 3);

  CHECK(e[3][0u].AsString() == str31);
  CHECK(e[3][1].AsString() == str32);
  CHECK(e[3][2].AsString() == str255);
}

TEST_CASE("rpc builder: str16 lower boundary") {
  auto ptr255 = MakeString(255);
  StringSlice str255(ptr255.Get(), 255);

  auto ptr256 = MakeString(256);
  StringSlice str256(ptr256.Get(), 256);

  RpcBuilder b(2, "str16_low_test");
  b.OpenShortArray();
  b.Add(str255);  // str8
  b.Add(str256);  // str16
  b.CloseShortArray();

  auto [e, chunks] = ParseFromBuilder(b.Finish());
  REQUIRE(e[3].Count() == 2);

  CHECK(e[3][0u].AsString() == str255);
  CHECK(e[3][1].AsString() == str256);
}

TEST_CASE("rpc builder: str16 range") {
  auto ptr256 = MakeString(256);
  StringSlice str256(ptr256.Get(), 256);

  auto ptr1024 = MakeString(1024);
  StringSlice str1024(ptr1024.Get(), 1024);

  auto ptr65535 = MakeString(65535);
  StringSlice str65535(ptr65535.Get(), 65535);

  RpcBuilder b(2, "str16_test");
  b.OpenShortArray();
  b.Add(str256);
  b.Add(str1024);
  b.Add(str65535);
  b.CloseShortArray();

  int fds[2];
  REQUIRE(pipe(fds) == 0);

  std::thread t([&]() {
    auto [data, bytes] = b.Finish();
    WriteAll(fds[1], data, bytes);
    close(fds[1]);
  });

  ByteStream stream(fds[0]);
  RpcChunkArrayPass pass(stream);
  StrongTypedView e = pass.Parse();
  auto chunks = pass.ReleaseChunks();

  REQUIRE(e[3].Count() == 3);

  CHECK(e[3][0u].AsString() == str256);
  CHECK(e[3][1].AsString() == str1024);
  CHECK(e[3][2].AsString() == str65535);

  t.join();
}

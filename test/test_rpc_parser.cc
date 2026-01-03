#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "parser/expr.h"
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

TEST_CASE("rpc notification: simple mode change") {
  int fds[2];
  REQUIRE(pipe(fds) == 0);

  // [2, "nvim_set_mode", ["i"]]
  const unsigned char msg[] = {0x93, 0x02, 0xAD, 'n', 'v', 'i', 'm',  '_',  's', 'e',
                               't',  '_',  'm',  'o', 'd', 'e', 0x91, 0xA1, 'i'};

  WriteAll(fds[1], msg, sizeof(msg));
  close(fds[1]);

  RpcPass pass(fds[0]);
  ExprView e(pass.Parse());

  CHECK(e[0].NumberOr(-1) == 2);
  CHECK(e[1].StringOr("X") == "nvim_set_mode");
  CHECK(e[2].Count() == 1);
  CHECK(e[2][0].StringOr("X") == "i");
}

TEST_CASE("rpc notification: buffer lines event") {
  int fds[2];
  REQUIRE(pipe(fds) == 0);

  // [2, "nvim_buf_lines_event", [1,0,10,10,["line1","line2"]]]
  const unsigned char msg[] = {0x93, 0x02, 0xB4, 'n',  'v',  'i',  'm',  '_',  'b', 'u', 'f',
                               '_',  'l',  'i',  'n',  'e',  's',  '_',  'e',  'v', 'e', 'n',
                               't',  0x95, 0x01, 0x00, 0x0A, 0x0A, 0x92, 0xA5, 'l', 'i', 'n',
                               'e',  '1',  0xA5, 'l',  'i',  'n',  'e',  '2'};

  WriteAll(fds[1], msg, sizeof(msg));
  close(fds[1]);

  RpcPass pass(fds[0]);
  ExprView e(pass.Parse());

  auto params = e[2];
  CHECK(params.Count() == 5);
  CHECK(params[0].NumberOr(-1) == 1);
  CHECK(params[4].Count() == 2);
  CHECK(params[4][0].StringOr("X") == "line1");
  CHECK(params[4][1].StringOr("X") == "line2");
}

TEST_CASE("rpc request from neovim") {
  int fds[2];
  REQUIRE(pipe(fds) == 0);

  // [0, 42, "my_plugin_do_thing", [123, "abc", true]]
  const unsigned char msg[] = {0x94, 0x00, 0x2A, 0xB2, 'm',  'y', '_', 'p', 'l', 'u',
                               'g',  'i',  'n',  '_',  'd',  'o', '_', 't', 'h', 'i',
                               'n',  'g',  0x93, 0x7B, 0xA3, 'a', 'b', 'c', 0xC3};

  WriteAll(fds[1], msg, sizeof(msg));
  close(fds[1]);

  RpcPass pass(fds[0]);
  ExprView e(pass.Parse());

  CHECK(e[0].NumberOr(-1) == 0);
  CHECK(e[1].NumberOr(-1) == 42);
  CHECK(e[2].StringOr("X") == "my_plugin_do_thing");

  auto params = e[3];
  CHECK(params.Count() == 3);
  CHECK(params[0].NumberOr(-1) == 123);
  CHECK(params[1].StringOr("X") == "abc");
}

TEST_CASE("rpc response with error") {
  int fds[2];
  REQUIRE(pipe(fds) == 0);

  // [1, 42, ["Invalid buffer", 0], nil]
  const unsigned char msg[] = {0x94, 0x01, 0x2A, 0x92, 0xAE, 'I', 'n', 'v', 'a',  'l', 'i',
                               'd',  ' ',  'b',  'u',  'f',  'f', 'e', 'r', 0x00, 0xC0};

  WriteAll(fds[1], msg, sizeof(msg));
  close(fds[1]);

  RpcPass pass(fds[0]);
  ExprView e(pass.Parse());

  CHECK(e[0].NumberOr(-1) == 1);
  CHECK(e[1].NumberOr(-1) == 42);

  auto err = e[2];
  CHECK(err.Count() == 2);
  CHECK(err[0].StringOr("X") == "Invalid buffer");
}

TEST_CASE("rpc back-to-back notifications") {
  int fds[2];
  REQUIRE(pipe(fds) == 0);

  // [2,"nvim_set_mode",["n"]]
  // ["i"]
  const unsigned char msg[] = {0x93, 0x02, 0xAD, 'n', 'v', 'i',  'm',  '_', 's',  'e',  't',
                               '_',  'm',  'o',  'd', 'e', 0x91, 0xA1, 'n', 0x91, 0xa1, 'i'};

  WriteAll(fds[1], msg, sizeof(msg));
  close(fds[1]);

  RpcPass pass(fds[0]);

  ExprView e1(pass.Parse());
  ExprView e2(pass.Parse());

  CHECK(e1[2][0].StringOr("X") == "n");
  CHECK(e2[0].StringOr("X") == "i");
}

TEST_CASE("rpc response with error as fixmap") {
  int fds[2];
  REQUIRE(pipe(fds) == 0);

  // [1, 7, {"message": "Invalid buffer", "code": 0}, nil]
  const unsigned char msg[] = {
      0x94,  // fixarray(4)
      0x01,  // response
      0x07,  // msgid = 7
      0x82,  // fixmap(2)

      0xA7, 'm', 'e', 's', 's', 'a', 'g', 'e',  // "message"
      0xAE, 'I', 'n', 'v', 'a', 'l', 'i', 'd',
      ' ',  'b', 'u', 'f', 'f', 'e', 'r',  // "Invalid buffer"

      0xA4, 'c', 'o', 'd', 'e',  // "code"
      0x00,                      // 0

      0xC0  // nil
  };

  WriteAll(fds[1], msg, sizeof(msg));
  close(fds[1]);

  RpcPass pass(fds[0]);
  ExprView e(pass.Parse());

  CHECK(e[0].NumberOr(-1) == 1);
  CHECK(e[1].NumberOr(-1) == 7);

  auto err = e[2];
  CHECK(err.Count() == 2);

  CHECK(err["message"].StringOr("X") == "Invalid buffer");
  CHECK(err["code"].NumberOr(-1) == 0);
}

#include <thread>
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "strings/byte_stream.h"

#include <fcntl.h>
#include <unistd.h>
#include <cstdint>
#include <cstring>

struct PipeStream {
  int rfd = -1;
  int wfd = -1;

  PipeStream() {
    int fds[2];
    REQUIRE(pipe(fds) == 0);
    rfd = fds[0];
    wfd = fds[1];
  }

  ~PipeStream() {
    if (rfd >= 0) {
      close(rfd);
    }
    if (wfd >= 0) {
      close(wfd);
    }
  }
};

TEST_CASE("ByteStream PopByte / PopUint8 / PopInt8") {
  PipeStream ps;

  uint8_t data[] = {0x01, 0x7f, 0xff};
  REQUIRE(write(ps.wfd, data, sizeof(data)) == sizeof(data));

  pdp::ByteStream bs(ps.rfd);

  CHECK(bs.PopByte() == 0x01);
  CHECK(bs.PopUint8() == 0x7f);
  CHECK(bs.PopInt8() == int8_t(0xff));
}

TEST_CASE("ByteStream big-endian integer parsing") {
  PipeStream ps;

  uint8_t data[] = {
      0x12, 0x34,                                     // uint16
      0x01, 0x02, 0x03, 0x04,                         // uint32
      0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08  // uint64
  };

  REQUIRE(write(ps.wfd, data, sizeof(data)) == sizeof(data));

  pdp::ByteStream bs(ps.rfd);

  CHECK(bs.PopUint16() == 0x1234);
  CHECK(bs.PopUint32() == 0x01020304);
  CHECK(bs.PopUint64() == 0x0102030405060708ULL);
}

TEST_CASE("More ByteStream big-endian parsing") {
  PipeStream ps;

  uint8_t data[] = {1, 2, 3, 4, 5, 6};
  REQUIRE(write(ps.wfd, data, sizeof(data)) == sizeof(data));

  pdp::ByteStream bs(ps.rfd);

  CHECK(bs.PopUint16() == 0x0102);
  CHECK(bs.PopUint16() == 0x0304);
  CHECK(bs.PopUint16() == 0x0506);
}

TEST_CASE("ByteStream Memcpy satisfied entirely from buffer") {
  PipeStream ps;

  uint8_t data[] = {10, 20, 30, 40};
  REQUIRE(write(ps.wfd, data, sizeof(data)) == sizeof(data));

  pdp::ByteStream bs(ps.rfd);

  uint8_t out[4] = {};
  bs.Memcpy(out, 4);

  CHECK(memcmp(out, data, 4) == 0);
}

TEST_CASE("ByteStream Memcpy crosses buffer boundary using ReadAtLeast") {
  PipeStream ps;

  uint8_t first[] = {1, 2, 3};
  uint8_t second[] = {4, 5, 6, 7, 8};

  REQUIRE(write(ps.wfd, first, sizeof(first)) == sizeof(first));
  REQUIRE(write(ps.wfd, second, sizeof(second)) == sizeof(second));

  pdp::ByteStream bs(ps.rfd);

  uint8_t out[8] = {};
  bs.Memcpy(out, 8);

  uint8_t expected[] = {1, 2, 3, 4, 5, 6, 7, 8};
  CHECK(memcmp(out, expected, 8) == 0);
}

TEST_CASE("ByteStream mixed Pop and Memcpy sequence") {
  PipeStream ps;

  uint8_t data[] = {
      0x00, 0x10,             // uint16
      0xaa, 0xbb, 0xcc, 0xdd  // payload
  };

  REQUIRE(write(ps.wfd, data, sizeof(data)) == sizeof(data));

  pdp::ByteStream bs(ps.rfd);

  CHECK(bs.PopUint16() == 0x0010);

  uint8_t payload[4] = {};
  bs.Memcpy(payload, 4);

  uint8_t expected[] = {0xaa, 0xbb, 0xcc, 0xdd};
  CHECK(memcmp(payload, expected, 4) == 0);
}

TEST_CASE("ByteStream Memcpy large size reads directly into destination") {
  PipeStream p;

  constexpr unsigned long long N = 1024;
  constexpr unsigned long long M = 64;

  uint8_t *src = (uint8_t *)malloc(N);
  for (size_t i = 0; i < N; ++i) {
    src[i] = i & 0xff;
  }

  std::thread writer([&] {
    for (size_t i = 0; i < M; ++i) {
      REQUIRE(write(p.wfd, src, N) == N);
    }
  });

  uint8_t *dst = (uint8_t *)malloc(N * M);
  memset(dst, 0x0, N * M);

  pdp::ByteStream bs(p.rfd);
  bs.Memcpy(dst, N * M);

  uint8_t *ptr = dst;
  for (size_t m = 0; m < M; ++m) {
    CHECK(memcmp(src, ptr, N) == 0);
    ptr += N;
  }

  writer.join();
  free(dst);
  free(src);
}

TEST_CASE("ByteStream Memcpy large includes leftover buffered bytes") {
  PipeStream p;

  constexpr unsigned long long N = 1024;
  constexpr unsigned long long M = 64;

  const uint8_t prefix[] = {0x01, 0x02};

  uint8_t *src = (uint8_t *)malloc(N);
  for (size_t i = 0; i < N; ++i) {
    src[i] = i & 0xff;
  }

  std::thread writer([&] {
    REQUIRE(write(p.wfd, prefix, sizeof(prefix)) == sizeof(prefix));
    for (size_t i = 0; i < M; ++i) {
      REQUIRE(write(p.wfd, src, N) == N);
    }
  });

  uint8_t *dst = (uint8_t *)malloc(sizeof(prefix) + N * M);
  memset(dst, 0x0, sizeof(prefix) + N * M);

  pdp::ByteStream bs(p.rfd);
  bs.Memcpy(dst, sizeof(prefix) + N * M);

  CHECK(memcmp(dst, prefix, sizeof(prefix)) == 0);
  uint8_t *ptr = dst + 2;
  for (size_t m = 0; m < M; ++m) {
    CHECK(memcmp(src, ptr, N) == 0);
    ptr += N;
  }

  writer.join();
  free(dst);
  free(src);
}

TEST_CASE("ByteStream Memcpy preserves scratch bytes with delayed write") {
  PipeStream p;
  pdp::ByteStream bs(p.rfd);

  const uint8_t scratch[] = {0xAA, 0xBB, 0xCC};
  REQUIRE(write(p.wfd, scratch, sizeof(scratch)) == (ssize_t)sizeof(scratch));

  // Consume only part of scratch
  CHECK(bs.PopByte() == 0xAA);

  const uint8_t payload[] = {1, 2, 3, 4, 5, 6, 7, 8};

  std::thread delayed_writer([&] {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    REQUIRE(write(p.wfd, payload, sizeof(payload)) == (ssize_t)sizeof(payload));
  });

  uint8_t dst[sizeof(scratch) - 1 + sizeof(payload)];
  bs.Memcpy(dst, sizeof(dst));

  // Remaining scratch bytes must come first
  CHECK(dst[0] == 0xBB);
  CHECK(dst[1] == 0xCC);

  for (size_t i = 0; i < sizeof(payload); ++i) {
    CHECK(dst[2 + i] == payload[i]);
  }

  delayed_writer.join();
}

TEST_CASE("ByteStream PopByte across multiple refills loses no data") {
  PipeStream p;

  constexpr size_t N = 10000;
  std::vector<uint8_t> src(N);
  for (size_t i = 0; i < N; ++i) {
    src[i] = static_cast<uint8_t>(i & 0xFF);
  }

  REQUIRE(write(p.wfd, src.data(), src.size()) == (ssize_t)src.size());

  pdp::ByteStream bs(p.rfd);

  for (size_t i = 0; i < N; ++i) {
    uint8_t v = bs.PopByte();
    CHECK(v == src[i]);
  }
}

TEST_CASE("ByteStream Skip interleaved with PopByte preserves stream order") {
  PipeStream p;

  constexpr size_t N = 1024;
  constexpr size_t M = 16;
  constexpr size_t TOTAL = N * M;

  uint8_t *src = (uint8_t *)malloc(TOTAL);
  for (size_t i = 0; i < TOTAL; ++i) {
    src[i] = i & 0xff;
  }

  std::thread writer([&] { REQUIRE(write(p.wfd, src, TOTAL) == (ssize_t)TOTAL); });

  pdp::ByteStream bs(p.rfd);

  size_t cursor = 0;

  // Pattern:
  //  - read 1 byte
  //  - skip N-1 bytes
  //  => every read should see src[cursor]
  for (size_t block = 0; block < M; ++block) {
    uint8_t b = bs.PopByte();
    CHECK(b == src[cursor]);

    cursor += 1;

    // Skip the rest of the block
    const size_t to_skip = N - 1;
    bs.Skip(to_skip);
    cursor += to_skip;
  }

  writer.join();
  free(src);
}

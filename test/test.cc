#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "rolling_buffer.h"
#include "string_builder.h"

TEST_CASE("StringView") {
  SUBCASE("Constructor") {
    pdp::StringSlice test("test");
    CHECK(test.Size() == 4);
    auto it = test.Begin();
    CHECK(*it == 't');
    ++it;
    CHECK(*it == 'e');
    ++it;
    CHECK(*it == 's');
    ++it;
    CHECK(*it == 't');
    ++it;
    CHECK(it == test.End());
  }

  SUBCASE("Fixed-length construction") {
    pdp::StringSlice test("test", 2);
    CHECK(test.Size() == 2);
    auto it = test.Begin();
    CHECK(*it == 't');
    ++it;
    CHECK(*it == 'e');
    ++it;
    CHECK(it == test.End());
  }

  SUBCASE("operator[]") {
    const char *sentance = "This is something I want to verify.";
    pdp::StringSlice s(sentance);

    size_t len = strlen(sentance);
    bool equal = true;
    for (size_t i = 0; i < len; ++i) {
      if (sentance[i] != s[i]) {
        equal = false;
      }
    }
    CHECK(equal);
  }
}

TEST_CASE("EstimateSize") {
  pdp::EstimateSize estimator;
  CHECK(estimator('d') >= 1);
  CHECK(estimator(13) >= 2);
  CHECK(estimator(-500) >= 4);
  CHECK(estimator("Test") >= 4);
  CHECK(estimator(0) >= 1);
  CHECK(estimator(-2147483648) >= 11);
  CHECK(estimator(9223372036854775807ll) >= 19);
  CHECK(estimator(std::numeric_limits<long long>::min()) >= 20);
  CHECK(estimator(18446744073709551615ull) >= 20);
}

TEST_CASE("StringBuilder") {
  SUBCASE("Append") {
    pdp::StringBuilder builder;
    builder.Append("What is the meaning");
    builder.Append(' ');
    builder.Append(-124);
    builder.Append(" or ");
    builder.Append(112);
    builder.Append('?');

    CHECK(builder == "What is the meaning -124 or 112?");
  }

  SUBCASE("Relocation") {
    pdp::StringBuilder builder;
    const char *what = "what";
    for (size_t i = 0; i < 1024; ++i) {
      builder.Append(what);
    }

    CHECK(builder.Size() == 1024 * 4);
    bool equal = true;
    size_t pos = 0;
    while (pos < 4096) {
      if (builder.Substr(pos, 4) != "what") {
        equal = false;
      }
      pos += 4;
    }
    CHECK(equal);
  }

  SUBCASE("Format") {
    pdp::StringBuilder builder;
    builder.Appendf("What is {} onn in here {} or {}{}", "going", 0, -101, '?');
    CHECK(builder == "What is going onn in here 0 or -101?");
  }
}

TEST_CASE("RollingBuffer") {
  SUBCASE("Read a sentance") {
    pdp::RollingBuffer buffer;

    int fds[2];
    pipe(fds);

    const char *message = "This is a sample sentance.";
    write(fds[1], message, strlen(message));
    close(fds[1]);

    size_t num_read = buffer.ReadFull(fds[0]);
    CHECK(num_read == strlen(message));
    auto s = buffer.ViewOnly();
    CHECK(s == message);
  }

  SUBCASE("Read a couple of sentances") {
    pdp::RollingBuffer buffer;

    int fds[2];
    pipe(fds);

    const char *message = "This is a sample sentance.\n";
    write(fds[1], message, strlen(message));

    const char *message2 = "This is another message.\n";
    write(fds[1], message2, strlen(message2));

    const char *message3 = "Final message sent.\n";
    write(fds[1], message3, strlen(message3));
    close(fds[1]);

    size_t num_read = buffer.ReadFull(fds[0]);
    CHECK(num_read > 0);
    CHECK(buffer.ConsumeLine() == message);
    CHECK(buffer.ConsumeLine() == message2);
    CHECK(buffer.ConsumeLine() == message3);
    CHECK(buffer.ConsumeLine().Empty());
  }
}

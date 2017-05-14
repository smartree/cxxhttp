/* Test cases for basic HTTP session handling.
 *
 * We use sample data to compare what the parser produced and what it should
 * have produced.
 *
 * See also:
 * * Project Documentation: https://ef.gy/documentation/cxxhttp
 * * Project Source Code: https://github.com/ef-gy/cxxhttp
 * * Licence Terms: https://github.com/ef-gy/cxxhttp/blob/master/COPYING
 *
 * @copyright
 * This file is part of the cxxhttp project, which is released as open source
 * under the terms of an MIT/X11-style licence, described in the COPYING file.
 */

#define ASIO_DISABLE_THREADS
#include <ef.gy/test-case.h>

#include <cxxhttp/http-session.h>

using namespace cxxhttp;

/* Test basic session attributes.
 * @log Test output stream.
 *
 * Test cases for some of the basic session functions.
 *
 * @return 'true' on success, 'false' otherwise.
 */
bool testBasicSession(std::ostream &log) {
  struct sampleData {
    enum http::status status;
    std::size_t requests, replies, contentLength;
    std::string content;
    std::size_t queries, remainingBytes;
    std::string buffer;
  };

  std::vector<sampleData> tests{
      {http::stRequest, 1, 2, 500, "foo", 3, 497, ""},
      {http::stContent, 1, 2, 500, "foo", 3, 497, ""},
      {http::stShutdown, 1, 2, 500, "foo", 3, 497, ""},
  };

  for (const auto &tt : tests) {
    http::sessionData v;

    v.status = tt.status;
    v.requests = tt.requests;
    v.replies = tt.replies;
    v.contentLength = tt.contentLength;
    v.content = tt.content;

    if (v.queries() != tt.queries) {
      log << "queries() = " << v.queries() << ", but expected " << tt.queries
          << "\n";
      return false;
    }

    if (v.remainingBytes() != tt.remainingBytes) {
      log << "queries() = " << v.remainingBytes() << ", but expected "
          << tt.remainingBytes << "\n";
      return false;
    }

    if (v.buffer() != tt.buffer) {
      log << "buffer() = " << v.buffer() << ", but expected " << tt.buffer
          << "\n";
      return false;
    }
  }

  return true;
}

/* Test log line creation.
 * @log Test output stream.
 *
 * Creates log lines for a few sample sessions.
 *
 * @return 'true' on success, 'false' otherwise.
 */
bool testLog(std::ostream &log) {
  struct sampleData {
    std::string address, request;
    http::headers header;
    int status;
    std::size_t length;
    std::string log;
  };

  std::vector<sampleData> tests{
      {"foo",
       "GET / HTTP/1.1",
       {},
       200,
       42,
       "foo - - [-] \"GET / HTTP/1.1\" 200 42 \"-\" \"-\""},
      {"[UNIX]",
       "GET / HTTP/1.1",
       {{"User-Agent", "frob/123"}},
       200,
       42,
       "[UNIX] - - [-] \"GET / HTTP/1.1\" 200 42 \"-\" \"frob/123\""},
      {"[UNIX]",
       "GET / HTTP/1.1",
       {{"User-Agent", "frob/123\"foo\""}},
       200,
       42,
       "[UNIX] - - [-] \"GET / HTTP/1.1\" 200 42 \"-\" \"(redacted)\""},
      {"[UNIX]",
       "GET / HTTP/1.1",
       {{"Referer", "http://foo/"}},
       200,
       42,
       "[UNIX] - - [-] \"GET / HTTP/1.1\" 200 42 \"http://foo/\" \"-\""},
      {"[UNIX]",
       "GET / HTTP/1.1",
       {{"Referer", "http://foo/%2"}},
       200,
       42,
       "[UNIX] - - [-] \"GET / HTTP/1.1\" 200 42 \"(invalid)\" \"-\""},
      {"[UNIX]",
       "GET / HTTP/1.1",
       {{"Referer", "http://foo/"}, {"User-Agent", "frob/123"}},
       200,
       42,
       "[UNIX] - - [-] \"GET / HTTP/1.1\" 200 42 \"http://foo/\" \"frob/123\""},
  };

  for (const auto &tt : tests) {
    http::sessionData s;

    s.inboundRequest = tt.request;
    s.header = tt.header;

    const auto &v = s.logMessage(tt.address, tt.status, tt.length);

    if (v != tt.log) {
      log << "logMessage() = '" << v << "', but expected '" << tt.log << "'\n";
      return false;
    }
  }

  return true;
}

/* Test request body creation.
 * @log Test output stream.
 *
 * Create sample replies for a few requests and verify them against known data.
 *
 * @return 'true' on success, 'false' otherwise.
 */
bool testReply(std::ostream &log) {
  struct sampleData {
    int status;
    http::headers header;
    std::string body, message;
  };

  std::vector<sampleData> tests{
      {100, {}, "", "HTTP/1.1 100 Continue\r\n\r\n"},
      {100, {}, "ignored", "HTTP/1.1 100 Continue\r\n\r\n"},
      {200, {}, "foo", "HTTP/1.1 200 OK\r\nContent-Length: 3\r\n\r\nfoo"},
      {404,
       {},
       "sorry",
       "HTTP/1.1 404 Not Found\r\nConnection: close\r\nContent-Length: "
       "5\r\n\r\nsorry"},
  };

  for (const auto &tt : tests) {
    http::sessionData s;

    const auto &v = s.generateReply(tt.status, tt.header, tt.body);

    if (v != tt.message) {
      log << "generateReply() = '" << v << "', but expected '" << tt.message
          << "'\n";
      return false;
    }
  }

  return true;
}

namespace test {
using efgy::test::function;

static function basicSession(testBasicSession);
static function log(testLog);
static function reply(testReply);
}

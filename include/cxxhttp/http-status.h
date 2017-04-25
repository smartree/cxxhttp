/* HTTP status line handling.
 *
 * For looking up status descriptions and parsing your favorite status lines.
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
#if !defined(CXXHTTP_HTTP_STATUS_H)
#define CXXHTTP_HTTP_STATUS_H

#include <regex>
#include <sstream>
#include <string>

#include <cxxhttp/http-constants.h>

namespace cxxhttp {
namespace http {
/* Get status code text description.
 * @status The HTTP status code to look up.
 *
 * This is a separate function, because we need to provide a default for the
 * map lookup.
 *
 * @return A text description, which can be sent with your HTTP request.
 */
static std::string statusDescription(unsigned status) {
  auto it = http::status.find(status);
  if (it != http::status.end()) {
    return it->second;
  }

  return "Other Status";
}

/* Broken out status line.
 *
 * Contains all the data in a status line, broken out into the relevant fields.
 */
class statusLine {
 public:
  /* Default initialiser.
   *
   * Initialise everything to be empty. Obviously this doesn't make for a valid
   * header.
   */
  statusLine(void) : code(0) {}

  /* Parse HTTP status line.
   * @line The (suspected) status line to parse.
   *
   * This currently only accepts HTTP/1.0 and HTTP/1.1 status lines, all others
   * will be rejected.
   */
  statusLine(const std::string &line) : code(0) {
    static const std::regex stat("(HTTP/1.[01])\\s+([0-9]{3})\\s+(.*)\\s*");
    std::smatch matches;
    bool matched = std::regex_match(line, matches, stat);

    if (matched) {
      protocol = matches[1];
      description = matches[3];
      try {
        code = std::stoi(matches[2]);
      } catch (...) {
        code = 500;
      }
    }
  }

  /* Create status line with status and protocol.
   * @pStatus The status code.
   * @pProtocol The protocol for the status line.
   *
   * Use this to create a status line when replying to a query.
   */
  statusLine(unsigned pStatus, const std::string &pProtocol = "HTTP/1.1")
      : code(pStatus),
        protocol(pProtocol),
        description(statusDescription(pStatus)) {}

  /* Did this status line parse correctly?
   *
   * Set to false, unless a successful parse happened (or the object has been
   * initialised directly, presumably with correct values).
   *
   * @return A boolean indicating whether or not this is a valid status line.
   */
  bool valid(void) const { return (code >= 100) && (code < 600); }

  /* The status code.
   *
   * Anything not in the http::status map is probably an error.
   */
  unsigned code;

  /* Protocol name.
   *
   * HTTP/1.0 or HTTP/1.1.
   */
  std::string protocol;

  /* Status code description.
   *
   * Should pretty much be ignored, and is only useful for humans reading a
   * stream transcript. And even then it can't really be trusted.
   */
  std::string description;

  /* Create status line.
   *
   * Uses whatever data we have to create a status line. In case the input would
   * not be valid, a generic status line is created.
   *
   * @return Returns the status line in a form that could be used in HTTP.
   */
  operator std::string(void) const {
    if (!valid()) {
      return "HTTP/1.1 500 Bad Status Line\r\n";
    }

    std::ostringstream s("");
    s << protocol << " " << code << " " << description << "\r\n";
    return s.str();
  }
};
}
}

#endif
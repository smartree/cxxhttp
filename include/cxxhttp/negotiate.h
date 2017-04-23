/* HTTP/1.1 Content Negotiation
 *
 * Implements a generic content negotiation algorithm as used in HTTP/1.1.
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
#if !defined(CXXHTTP_NEGOTIATE_H)
#define CXXHTTP_NEGOTIATE_H

#include <algorithm>
#include <cmath>
#include <regex>
#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace cxxhttp {
/* std::isspace wrapper for trim().
 * @c The character to check.
 *
 * Reports whether 'c' is a whitespace character.
 *
 * @return true if c is a whitepace character.
 */
static inline bool isspace(int c) { return std::isspace(c); }

/* Trim whitespace on either side of string.
 * @s The string to trim.
 *
 * This function creates a new copy of a string that has whitespace on either
 * side of it removed.
 *
 * @return 's', without whitespace on either side.
 */
static inline std::string trim(const std::string &s) {
  return std::string(std::find_if_not(s.begin(), s.end(), isspace),
                     std::find_if_not(s.rbegin(), s.rend(), isspace).base());
}

/* Split string by delimiter.
 * @list The string to split.
 * @sep The separator to use.
 *
 * This splits up a string based on the given delimiter. The resulting strings
 * are also trimmed.
 *
 * @return A vector of list elements in 'list', split by 'sep'.
 */
static inline std::vector<std::string> split(const std::string &list,
                                             const char sep = ',') {
  std::vector<std::string> rv;
  std::istringstream stream(list);
  std::string item;
  while (std::getline(stream, item, sep)) {
    rv.push_back(trim(item));
  }
  return rv;
}

/* Container type for quality-tagged values.
 *
 * q-values are used throughout HTTP for content negotiation. This type
 * encapsulates such types, so we can do content negotiation based on them.
 */
class qvalue {
 public:
  /* Construct from encoded string
   * @val The value to parse.
   *
   * This constructor gets a fully-qualified string and returns the parsed
   * version of it. The value is of the form 'foo(;attr)*(;q=D.DDD)?(;ext)*' as
   * described in RFC 2616.
   */
  qvalue(const std::string &val) : q(-1) {
    for (const auto &s : split(val, ';')) {
      if (value.empty()) {
        // if we haven't got a base value yet, use the current segment as such.
        value = s;
      } else if (q == -1) {
        static const std::regex rx("q\\s*=\\s*([01](\\.[0-9]{0,3})?)");
        std::smatch matches;
        // parse a q-value, if the current segment is one.
        if (std::regex_match(s, matches, rx)) {
          float f = std::stof(matches[1]);
          q = std::floor(f * 1000);
        } else {
          // append to the normal types attributes if not.
          attributes.insert(s);
        }
      } else {
        // if we already have a q-value, then this is part of an
        // accept-extension or similar, so put the value there.
        extensions.insert(s);
      }
    }

    if (!value.empty() && (q == -1)) {
      q = 1000;
    }

    // limit q-values between 0 and 1000.
    q = std::max(std::min(q, 1000), 0);
  }

  /* Value
   *
   * This is the main value with which the quality value below is associated. In
   * HTTP/1.1, this would be a single element in a list of MIME types, or a
   * language code or similar.
   */
  std::string value;

  /* Attributes for value.
   *
   * If 'value' is a MIME type, then this would be additional attributes for
   * this MIME type. RFC 2616 has an example of 'level=1' for type 'text/html',
   * which would be written as 'text/html;level=1'.
   */
  std::set<std::string> attributes;

  /* Extensions
   *
   * Fields like "Accept" allow for extension attributes, which come after the
   * q= attribute and would not be part of the MIME type or similar value. There
   * were no examples in the standard, so this is pretty generous in what it
   * allows.
   *
   * It was also unclear in the standard if the order of these parameters would
   * matter and whether deduplication was allowed, so this is using a std::set
   * which violates both of these properties, since it didn't seem otherwise.
   */
  std::set<std::string> extensions;

  /* Quality value.
   *
   * This is the 'quality' value, or q-value associated with the entry. See RFC
   * 2616, section 3.9 for a summary. The range of these is 0.000 - 1.000, with
   * 3 digits max after the decimal point, so the're represented here as an int
   * with a range of 0 - 1000, defaulting to 1000.
   *
   * See: https://www.w3.org/Protocols/rfc2616/rfc2616-sec3.html#sec3.9
   */
  int q;

  /* Recombined value.
   *
   * This will recombine the value with the attributes, if there are any, by
   * appending any attributes to the value separated by a semicolon.
   *
   * Note that the order of the attributes here might be different than in the
   * input, because we put them in a std::set which will likely be sorted
   * alphabetically. Since MIME type attribues are sort of rare to begin with,
   * and I couldn't find any where the ordering mattered, I assumed this will be
   * grand.
   *
   * @return A string of the form 'value(;attribute)*'.
   */
  operator std::string(void) const {
    std::string rv = value;
    if (!rv.empty()) {
      for (const auto &a : attributes) {
        rv += ";" + a;
      }
    }
    return rv;
  }

  /* Full recombined value.
   *
   * Like the std::string conversion operator, but including the q-value and any
   * extension attributes.
   *
   * @return A string of the form 'value(;attribute)*;q=D.DDD(;ext)*'.
   */
  std::string full(void) const {
    std::stringstream qv;
    std::string rv = *this;
    if (!rv.empty()) {
      qv << (q / 1000.);
      rv += ";q=" + qv.str();
      for (const auto &a : extensions) {
        rv += ";" + a;
      }
    }
    return rv;
  }

  /* Order qvalues.
   * @b The value to compare to.
   *
   * RFC 2616 imposes a specific order on these qvalues. The primary sorting is
   * via the quality value, but as tie breakers the spec (e.g. in section 14.1)
   * that more specific entries are to be preferred over less spcific ones. This
   * includes attributes and, if the value is a mime type, then it's important
   * whether these use the * operator to match several types.
   *
   * See: https://www.w3.org/Protocols/rfc2616/rfc2616-sec14.html#sec14.1
   *
   * @return true if the value is less than b's value, taking into account the
   * preferences expressed.
   */
  bool operator<(const qvalue &b) const {
    if (q < b.q) {
      return true;
    } else if (b.q < q) {
      return false;
    }

    if (value == b.value) {
      if (attributes.size() < b.attributes.size()) {
        return true;
      }
    }

    // split the values by the '/' character - this is for mime types.
    auto sa = split(value, '/');
    auto sb = split(b.value, '/');

    if (sa.size() != sb.size()) {
      // this is technically an error, but we'll roll with it and assume the
      // side with fewer components wins.
      return sa.size() < sb.size();
    }

    if (sa.size() == 2) {
      // we're most likely looking at MIME types. Carry on.
      if (sa[0] == "*") {
        // only allowed if sa[1] == "*" as well. We'll just assume that's true.
        // In that case, if b is not also */* then b is greater than this.
        return sb[0] != "*";
      } else if (sb[0] == "*") {
        // if b has a value of */*, then that one is less than this one.
        return false;
      } else if (sa[0] == sb[0]) {
        // if the main type of both is the same, see if either is *.
        return sa[1] == "*";
      }
    } else if (sa.size() == 1) {
      // the two values have no '/' characters, so are most likely not mime
      // types. The only other kind of value where we care about sub-parts in
      // the standard is language tags, which can have an arbitrary number of
      // sub-tags on either side.
      auto sa = split(value, '-');
      auto sb = split(b.value, '-');

      for (unsigned i = 0; i < std::min(sa.size(), sb.size()); i++) {
        if (sa[i] != sb[i]) {
          return sa[i] < sb[i];
        }
      }
      // if we get here, that means that up until the shortest sub-tag the two
      // were identical. So the the longer one is more specific, thus the
      // shorter one is less than the longer one.
      return sa.size() < sb.size();
    }

    // We couldn't find out which is less than the other, so just default to
    // lexical sorting of the recombined string.
    return std::string(*this) < std::string(b);
  }

  /* Report whether two qvalue values match.
   * @b The value to compare to.
   *
   * This does not take into account the quality value, only the actual value
   * and its attributes. If either side is a '*' then that is also considered
   * a match. This also allows MIME-type matching, if both sides have a / in
   * them.
   *
   * For fuzzy matches (with *), the attributes are ignored. Otherwise they have
   * to match precisely. The logic here is that the fuzzy matching already
   * implies that we can't know the valid attributes anyway, so we ignore them.
   *
   * @return true if the values are equal, allowing for wildcards.
   */
  bool operator==(const qvalue &b) const {
    bool valueMatch = value == b.value;
    bool attributesMatch = attributes == b.attributes;

    if (valueMatch && attributesMatch) {
      return true;
    }

    bool aw = hasWildcard();
    bool bw = b.hasWildcard();

    if (aw == bw) {
      // if both sides are, or have, a wildcard, then this can't match.
      return false;
    }

    auto sa = split(value, '/');
    auto sb = split(b.value, '/');

    if ((sa.size() == sb.size()) && !sa.empty()) {
      // if both sides have the same number of /-es (we really assume there's
      // only two components here) then we have a match if the subcomponents are
      // all the same or a wild card.
      for (unsigned i = 0; i < sa.size(); i++) {
        if ((sa[i] != sb[i]) && (sa[i] != "*") && (sb[i] != "*")) {
          return false;
        }
      }

      // if we get here, then each component has matched one way or another.
      return true;
    }

    return false;
  }

  /* Report whether value has a wildcard or a wildcard component.
   *
   * Wildcard matching requires some shenanigans. Generally, elements with
   * wildcards can't be used as the result in negotiations directly, and have
   * lower precedence than non-wildcard values.
   *
   * @return 'true' if the value has a wildcard.
   */
  bool hasWildcard(void) const {
    auto a = split(value, '/');
    return std::find(a.begin(), a.end(), "*") != a.end();
  }
};

/* Negotiate with quality-value.
 * @theirs The client's list of acceptable values.
 * @mine The server's list of acceptable values.
 *
 * Implements the HTTP/1.1 content negotiation as described in RFC 2616, section
 * 14. See the RFC for extra details.
 *
 * The algorithm is basically the same for all the headers, so is only
 * implemented once. As a minor extension to the standard, this allows for
 * q-values from both the client and the server. If none are specified on the
 * server side, then the effect is the same as in the standard, otherwise the
 * values are multiplied together, thus allowing the server to influence the
 * choice in selected values as well.
 *
 * Also, the function will never return a value that has a wildcard. This may
 * mean that there's a match between both sides in principle, but nothing is
 * returned because of wildcards on both sides.
 *
 * See: https://www.w3.org/Protocols/rfc2616/rfc2616-sec14.html#sec14.1
 *
 * @return The negotiated value.
 */
static inline std::string negotiate(std::set<qvalue> theirs,
                                    std::set<qvalue> mine) {
  if (mine.size() == 0) {
    // this branch indicates a programming error on the server side. There's no
    // use in negotiating the content if we don't know what we want.
    return "";
  }

  if (theirs.size() == 0) {
    // the user didn't specify anything, so go with the highest-preference on
    // our side.
    std::vector<qvalue> rev(mine.rbegin(), mine.rend());
    for (const auto &v : rev) {
      if (!v.hasWildcard()) {
        // only return a value if there's no wildcards.
        return v;
      }
    }

    // apparently we only had wildcards, which again is a programming error. the
    // result is no match.
    return "";
  }

  std::set<qvalue> is;

  // this is to intersect the two sets. We can't use std::set_intersect, because
  // we also need to recalculate the q-values.
  for (const auto &a : theirs) {
    for (const auto &b : mine) {
      if (a == b) {
        // Combined q-value. Combining like this allows for server-side q-value
        // influences.
        int q = a.q * b.q / 1000;

        if (a.hasWildcard()) {
          auto qv = b;
          qv.q = q;
          is.insert(qv);
        } else if (b.hasWildcard()) {
          auto qv = a;
          qv.q = q;
          is.insert(qv);
        } else {
          auto qv = b;
          qv.q = q;
          is.insert(qv);
        }
      }
    }
  }

  if (is.size() == 0) {
    // no matches between the two sets, so... oops.
    return "";
  }

  // since everything was sorted, and only matches are left, the highest-quality
  // value is the one we want.
  return *(is.rbegin());
}

/* Negotiate with quality-value.
 * @theirs The client's list of acceptable values.
 * @mine The server's list of acceptable values.
 *
 * This is the std::vector version of the negotiation function. See the std::set
 * variant for more details on the algorithm.
 *
 * @return The negotiated value.
 */
static inline std::string negotiate(std::vector<std::string> theirs,
                                    std::vector<std::string> mine) {
  return negotiate(std::set<qvalue>(theirs.begin(), theirs.end()),
                   std::set<qvalue>(mine.begin(), mine.end()));
}

/* Negotiate with quality-value.
 * @theirs The client's list of acceptable values.
 * @mine The server's list of acceptable values.
 *
 * This is the string version of the negotiation function. See the std::set
 * variant for more details on the algorithm.
 *
 * @return The negotiated value.
 */
static inline std::string negotiate(const std::string &theirs,
                                    const std::string &mine) {
  return negotiate(split(theirs), split(mine));
}
}

#endif

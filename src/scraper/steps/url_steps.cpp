#include "scraper/steps/url_steps.hpp"
#include <absl/container/flat_hash_map.h>
#include <absl/strings/ascii.h>
#include <absl/strings/str_join.h>
#include <absl/strings/str_split.h>
#include <libxml/uri.h>
#include <libxml/xmlmemory.h>
#include <limits>
#include <memory>
#include <stdexcept>

namespace scraper {

namespace {

struct XmlCharDeleter {
  void operator()(xmlChar *p) const noexcept {
    if (p) {
      xmlFree(p);
    }
  }
};

struct CharDeleter {
  void operator()(char *p) const noexcept {
    if (p) {
      xmlFree(p);
    }
  }
};

struct XmlUriDeleter {
  void operator()(xmlURI *p) const noexcept {
    if (p) {
      xmlFreeURI(p);
    }
  }
};

using XmlStringPtr = std::unique_ptr<xmlChar, XmlCharDeleter>;
using XmlCharBufPtr = std::unique_ptr<char, CharDeleter>;
using XmlUriPtr = std::unique_ptr<xmlURI, XmlUriDeleter>;

std::string decode_uri_component(std::string_view encoded) {
  if (encoded.empty()) {
    return "";
  }
  if (encoded.size() > static_cast<size_t>(std::numeric_limits<int>::max())) {
    throw std::overflow_error("decode_uri_component: input exceeds INT_MAX");
  }
  // Replace '+' with ' ' for application/x-www-form-urlencoded query params.
  std::string tmp(encoded);
  for (char &c : tmp) {
    if (c == '+') {
      c = ' ';
    }
  }
  XmlCharBufPtr unescaped(
      xmlURIUnescapeString(tmp.c_str(), static_cast<int>(tmp.size()), nullptr));
  if (!unescaped) {
    return tmp;
  }
  return std::string(unescaped.get());
}

// Extracts raw query string tolerating unencoded '[' / ']' that libxml2's
// RFC 3986 xmlParseURI rejects.
std::string_view extract_raw_query(std::string_view url, XmlUriPtr &uri_holder) {
  uri_holder.reset(xmlParseURI(std::string(url).c_str()));
  if (uri_holder) {
    const char *raw_q =
        uri_holder->query_raw ? uri_holder->query_raw : uri_holder->query;
    return raw_q ? std::string_view(raw_q) : std::string_view{};
  }
  const size_t q = url.find('?');
  if (q == std::string_view::npos) {
    return {};
  }
  const size_t hash = url.find('#', q + 1);
  return (hash == std::string_view::npos)
             ? url.substr(q + 1)
             : url.substr(q + 1, hash - (q + 1));
}

bool glob_match(std::string_view pattern, std::string_view text) {
  size_t p = 0, t = 0;
  size_t star_p = std::string_view::npos, star_t = 0;
  while (t < text.size()) {
    if (p < pattern.size() && pattern[p] == '*') {
      star_p = p++;
      star_t = t;
    } else if (p < pattern.size() && pattern[p] == text[t]) {
      ++p;
      ++t;
    } else if (star_p != std::string_view::npos) {
      p = star_p + 1;
      t = ++star_t;
    } else {
      return false;
    }
  }
  while (p < pattern.size() && pattern[p] == '*') {
    ++p;
  }
  return p == pattern.size();
}

bool matches_any_glob(const std::vector<std::string> &patterns,
                      std::string_view key) {
  for (const auto &pat : patterns) {
    if (glob_match(pat, key)) {
      return true;
    }
  }
  return false;
}

bool domain_matches(std::string_view host, std::string_view pattern) {
  if (pattern.empty()) {
    return false;
  }
  if (host == pattern) {
    return true;
  }
  return host.size() > pattern.size() &&
         host[host.size() - pattern.size() - 1] == '.' &&
         host.ends_with(pattern);
}

struct ParsedOrigin {
  std::string scheme;
  std::string host;
  int port = 0;
  bool valid = false;
};

ParsedOrigin parse_url_origin(std::string_view url) {
  ParsedOrigin out;
  XmlUriPtr uri(xmlParseURI(std::string(url).c_str()));
  if (uri && uri->scheme && uri->server) {
    out.scheme = absl::AsciiStrToLower(uri->scheme);
    out.host = absl::AsciiStrToLower(uri->server);
    out.port = uri->port > 0 ? uri->port : 0;
    out.valid = true;
    return out;
  }
  // Fallback for URLs with unencoded '['/']' in query
  const size_t scheme_sep = url.find("://");
  if (scheme_sep != std::string_view::npos && scheme_sep > 0) {
    out.scheme = absl::AsciiStrToLower(url.substr(0, scheme_sep));
    const size_t auth_start = scheme_sep + 3;
    const size_t auth_end = url.find_first_of("/?#", auth_start);
    std::string_view auth =
        (auth_end == std::string_view::npos)
            ? url.substr(auth_start)
            : url.substr(auth_start, auth_end - auth_start);
    if (const size_t at = auth.rfind('@'); at != std::string_view::npos) {
      auth = auth.substr(at + 1);
    }
    if (!auth.empty() && auth.front() == '[') {
      const size_t rb = auth.find(']');
      if (rb != std::string_view::npos) {
        out.host = absl::AsciiStrToLower(auth.substr(0, rb + 1));
        if (rb + 2 <= auth.size() && auth[rb + 1] == ':') {
          int p = 0;
          for (char c : auth.substr(rb + 2)) {
            if (c >= '0' && c <= '9') {
              p = p * 10 + (c - '0');
            }
          }
          out.port = p;
        }
        out.valid = true;
      }
    } else if (!auth.empty()) {
      const size_t colon = auth.find(':');
      out.host = absl::AsciiStrToLower(
          colon == std::string_view::npos ? auth : auth.substr(0, colon));
      if (colon != std::string_view::npos) {
        int p = 0;
        for (char c : auth.substr(colon + 1)) {
          if (c >= '0' && c <= '9') {
            p = p * 10 + (c - '0');
          }
        }
        out.port = p;
      }
      out.valid = !out.host.empty();
    }
  } else if (uri && uri->scheme) {
    out.scheme = absl::AsciiStrToLower(uri->scheme);
    out.valid = true;
  }
  return out;
}

} // namespace

void ResolveUrlStep::apply(Node, std::vector<std::string> &values,
                           jsoncons::json &) {
  if (base_url_.empty()) {
    return;
  }
  for (auto &v : values) {
    if (v.empty()) {
      continue;
    }
    XmlStringPtr resolved(
        xmlBuildURI(BAD_CAST v.c_str(), BAD_CAST base_url_.c_str()));
    if (resolved) {
      v = reinterpret_cast<const char *>(resolved.get());
    }
  }
}

UrlPartStep::UrlPartStep(std::string_view part) {
  const std::string lower = absl::AsciiStrToLower(part);
  if (lower == "scheme") {
    part_ = Part::Scheme;
  } else if (lower == "host") {
    part_ = Part::Host;
  } else if (lower == "port") {
    part_ = Part::Port;
  } else if (lower == "path") {
    part_ = Part::Path;
  } else if (lower == "query") {
    part_ = Part::Query;
  } else if (lower == "fragment") {
    part_ = Part::Fragment;
  } else {
    throw std::runtime_error("Unknown URL part: " + std::string(part));
  }
}

void UrlPartStep::apply(Node, std::vector<std::string> &values,
                        jsoncons::json &) {
  for (auto &v : values) {
    XmlUriPtr uri(xmlParseURI(v.c_str()));
    if (!uri) {
      v.clear();
      continue;
    }
    switch (part_) {
    case Part::Scheme:
      v = uri->scheme ? uri->scheme : "";
      break;
    case Part::Host:
      v = uri->server ? uri->server : "";
      break;
    case Part::Port:
      v = uri->port > 0 ? std::to_string(uri->port) : "";
      break;
    case Part::Path:
      v = uri->path ? uri->path : "";
      break;
    case Part::Query:
      v = uri->query_raw ? uri->query_raw : (uri->query ? uri->query : "");
      break;
    case Part::Fragment:
      v = uri->fragment ? uri->fragment : "";
      break;
    }
  }
}

void QueryParamStep::apply(Node, std::vector<std::string> &values,
                           jsoncons::json &) {
  for (auto &v : values) {
    XmlUriPtr uri;
    const std::string_view raw_q = extract_raw_query(v, uri);
    if (raw_q.empty()) {
      v = default_val_;
      continue;
    }

    std::string found = default_val_;
    for (std::string_view pair :
         absl::StrSplit(raw_q, '&', absl::SkipEmpty())) {
      const size_t eq = pair.find('=');
      const std::string_view raw_k =
          (eq == std::string_view::npos) ? pair : pair.substr(0, eq);
      const std::string_view raw_val =
          (eq == std::string_view::npos) ? std::string_view{}
                                         : pair.substr(eq + 1);
      if (decode_uri_component(raw_k) == param_name_) {
        found = decode_uri_component(raw_val);
        break;
      }
    }
    v = std::move(found);
  }
}

void QueryParamsStep::apply(Node, std::vector<std::string> &values,
                            jsoncons::json &dict) {
  if (values.empty()) {
    return;
  }

  std::vector<std::vector<std::string>> collected(param_to_key_.size());
  for (const auto &v : values) {
    if (v.empty()) {
      continue;
    }
    XmlUriPtr uri;
    const std::string_view raw_q = extract_raw_query(v, uri);
    absl::flat_hash_map<std::string, std::string> parsed;
    if (!raw_q.empty()) {
      for (std::string_view pair :
           absl::StrSplit(raw_q, '&', absl::SkipEmpty())) {
        const size_t eq = pair.find('=');
        const std::string_view raw_k =
            (eq == std::string_view::npos) ? pair : pair.substr(0, eq);
        const std::string_view raw_val =
            (eq == std::string_view::npos) ? std::string_view{}
                                           : pair.substr(eq + 1);
        parsed.try_emplace(decode_uri_component(raw_k),
                           decode_uri_component(raw_val));
      }
    }

    for (size_t i = 0; i < param_to_key_.size(); ++i) {
      const auto it = parsed.find(param_to_key_[i].first);
      if (it != parsed.end()) {
        collected[i].push_back(it->second);
      } else if (!default_val_.empty()) {
        collected[i].push_back(default_val_);
      }
    }
  }

  for (size_t i = 0; i < param_to_key_.size(); ++i) {
    const auto &items = collected[i];
    if (items.empty()) {
      continue;
    }
    const std::string &target_key = param_to_key_[i].second;
    if (items.size() == 1) {
      insert_into_json(dict, target_key, jsoncons::json(items[0]), opts_);
    } else {
      jsoncons::json arr = jsoncons::json::array();
      for (const auto &item : items) {
        arr.push_back(item);
      }
      insert_into_json(dict, target_key, std::move(arr), opts_);
    }
  }

  if (clear_) {
    values.clear();
  }
}

void StripQueryStep::apply(Node, std::vector<std::string> &values,
                           jsoncons::json &) {
  for (auto &v : values) {
    auto filter_query = [&](std::string_view raw_q) -> std::string {
      if (keep_params_.empty() && drop_params_.empty()) {
        return {};
      }
      std::vector<std::string_view> kept;
      for (std::string_view pair :
           absl::StrSplit(raw_q, '&', absl::SkipEmpty())) {
        const size_t eq = pair.find('=');
        const std::string_view raw_k =
            (eq == std::string_view::npos) ? pair : pair.substr(0, eq);
        const std::string key = decode_uri_component(raw_k);
        const bool keep_ok =
            keep_params_.empty() || matches_any_glob(keep_params_, key);
        const bool drop_hit =
            !drop_params_.empty() && matches_any_glob(drop_params_, key);
        if (keep_ok && !drop_hit) {
          kept.push_back(pair);
        }
      }
      return kept.empty() ? std::string{} : absl::StrJoin(kept, "&");
    };

    XmlUriPtr uri(xmlParseURI(v.c_str()));
    if (!uri) {
      const size_t q_pos = v.find('?');
      if (q_pos == std::string::npos) {
        continue;
      }
      const size_t hash_pos = v.find('#', q_pos + 1);
      const std::string_view raw_q =
          (hash_pos == std::string::npos)
              ? std::string_view(v).substr(q_pos + 1)
              : std::string_view(v).substr(q_pos + 1, hash_pos - (q_pos + 1));
      const std::string joined = filter_query(raw_q);
      std::string out = v.substr(0, q_pos);
      if (!joined.empty()) {
        out.push_back('?');
        out.append(joined);
      }
      if (hash_pos != std::string::npos) {
        out.append(v, hash_pos, std::string::npos);
      }
      v = std::move(out);
      continue;
    }

    const char *raw_q = uri->query_raw ? uri->query_raw : uri->query;
    const std::string joined = raw_q ? filter_query(raw_q) : std::string{};
    if (uri->query) {
      xmlFree(uri->query);
      uri->query = nullptr;
    }
    if (uri->query_raw) {
      xmlFree(uri->query_raw);
      uri->query_raw = nullptr;
    }
    if (!joined.empty()) {
      uri->query_raw =
          reinterpret_cast<char *>(xmlCharStrdup(joined.c_str()));
    }

    XmlStringPtr saved(xmlSaveUri(uri.get()));
    if (saved) {
      v = reinterpret_cast<const char *>(saved.get());
    }
  }
}

FilterUrlsStep::FilterUrlsStep(std::vector<std::string> allowed_domains,
                               std::vector<std::string> blocked_domains,
                               std::vector<std::string> allowed_schemes,
                               std::string_view same_origin_of) {
  allowed_domains_.reserve(allowed_domains.size());
  for (const auto &d : allowed_domains) {
    allowed_domains_.push_back(absl::AsciiStrToLower(d));
  }
  blocked_domains_.reserve(blocked_domains.size());
  for (const auto &d : blocked_domains) {
    blocked_domains_.push_back(absl::AsciiStrToLower(d));
  }
  for (const auto &s : allowed_schemes) {
    allowed_schemes_.insert(absl::AsciiStrToLower(s));
  }
  if (!same_origin_of.empty()) {
    const ParsedOrigin origin = parse_url_origin(same_origin_of);
    if (!origin.valid || origin.host.empty()) {
      throw std::invalid_argument(
          "filter_urls: invalid same_origin_of URL: " +
          std::string(same_origin_of));
    }
    same_origin_scheme_ = origin.scheme;
    same_origin_host_ = origin.host;
    same_origin_port_ = origin.port;
    check_same_origin_ = true;
  }
}

void FilterUrlsStep::apply(Node, std::vector<std::string> &values,
                           jsoncons::json &) {
  std::vector<std::string> kept;
  kept.reserve(values.size());
  for (auto &v : values) {
    if (v.empty()) {
      continue;
    }
    const ParsedOrigin parsed = parse_url_origin(v);
    if (!parsed.valid) {
      continue;
    }
    if (!allowed_schemes_.empty() &&
        (parsed.scheme.empty() || !allowed_schemes_.contains(parsed.scheme))) {
      continue;
    }
    if (check_same_origin_ &&
        (parsed.scheme != same_origin_scheme_ ||
         parsed.host != same_origin_host_ ||
         parsed.port != same_origin_port_)) {
      continue;
    }
    if (!allowed_domains_.empty()) {
      bool ok = false;
      for (const auto &d : allowed_domains_) {
        if (domain_matches(parsed.host, d)) {
          ok = true;
          break;
        }
      }
      if (!ok) {
        continue;
      }
    }
    bool blocked = false;
    for (const auto &d : blocked_domains_) {
      if (domain_matches(parsed.host, d)) {
        blocked = true;
        break;
      }
    }
    if (blocked) {
      continue;
    }
    kept.push_back(std::move(v));
  }
  values = std::move(kept);
}

void SlugifyStep::apply(Node, std::vector<std::string> &values,
                        jsoncons::json &) {
  for (auto &v : values) {
    const std::string lower = absl::AsciiStrToLower(v);
    std::string out;
    out.reserve(lower.size());
    bool prev_sep = true; // suppresses leading separator

    for (unsigned char ch : lower) {
      if ((ch >= 'a' && ch <= 'z') || (ch >= '0' && ch <= '9')) {
        out.push_back(static_cast<char>(ch));
        prev_sep = false;
      } else if (!prev_sep && !separator_.empty()) {
        out.append(separator_);
        prev_sep = true;
      }
    }

    // Trim trailing separator if present.
    if (!separator_.empty() && out.size() >= separator_.size() &&
        out.compare(out.size() - separator_.size(), separator_.size(),
                    separator_) == 0) {
      out.resize(out.size() - separator_.size());
    }

    if (max_len_.has_value() && out.size() > *max_len_) {
      out.resize(*max_len_);
      if (!separator_.empty() && out.size() >= separator_.size() &&
          out.compare(out.size() - separator_.size(), separator_.size(),
                      separator_) == 0) {
        out.resize(out.size() - separator_.size());
      }
    }

    v = std::move(out);
  }
}

} // namespace scraper

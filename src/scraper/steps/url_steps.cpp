#include "scraper/steps/url_steps.hpp"
#include <absl/strings/ascii.h>
#include <absl/strings/str_join.h>
#include <absl/strings/str_split.h>
#include <libxml/uri.h>
#include <libxml/xmlmemory.h>
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

struct XmlUriDeleter {
  void operator()(xmlURI *p) const noexcept {
    if (p) {
      xmlFreeURI(p);
    }
  }
};

using XmlStringPtr = std::unique_ptr<xmlChar, XmlCharDeleter>;
using XmlUriPtr = std::unique_ptr<xmlURI, XmlUriDeleter>;

std::string decode_uri_component(std::string_view encoded) {
  if (encoded.empty()) {
    return "";
  }
  // Replace '+' with ' ' for application/x-www-form-urlencoded query params.
  std::string tmp(encoded);
  for (char &c : tmp) {
    if (c == '+') {
      c = ' ';
    }
  }
  char *unescaped = xmlURIUnescapeString(tmp.c_str(), static_cast<int>(tmp.size()), nullptr);
  if (!unescaped) {
    return tmp;
  }
  std::string result(unescaped);
  xmlFree(unescaped);
  return result;
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
    XmlUriPtr uri(xmlParseURI(v.c_str()));
    const char *raw_q =
        uri ? (uri->query_raw ? uri->query_raw : uri->query) : nullptr;
    if (!raw_q) {
      v = default_val_;
      continue;
    }

    std::string found = default_val_;
    for (std::string_view pair : absl::StrSplit(raw_q, '&', absl::SkipEmpty())) {
      const size_t eq = pair.find('=');
      const std::string_view raw_k =
          (eq == std::string_view::npos) ? pair : pair.substr(0, eq);
      const std::string_view raw_val =
          (eq == std::string_view::npos) ? std::string_view{} : pair.substr(eq + 1);
      if (decode_uri_component(raw_k) == param_name_) {
        found = decode_uri_component(raw_val);
        break;
      }
    }
    v = std::move(found);
  }
}

void StripQueryStep::apply(Node, std::vector<std::string> &values,
                           jsoncons::json &) {
  for (auto &v : values) {
    XmlUriPtr uri(xmlParseURI(v.c_str()));
    if (!uri) {
      continue;
    }
    if (keep_params_.empty()) {
      if (uri->query) {
        xmlFree(uri->query);
        uri->query = nullptr;
      }
      if (uri->query_raw) {
        xmlFree(uri->query_raw);
        uri->query_raw = nullptr;
      }
    } else {
      const char *raw_q = uri->query_raw ? uri->query_raw : uri->query;
      if (raw_q) {
        std::vector<std::string_view> kept;
        for (std::string_view pair :
             absl::StrSplit(raw_q, '&', absl::SkipEmpty())) {
          const size_t eq = pair.find('=');
          const std::string_view raw_k =
              (eq == std::string_view::npos) ? pair : pair.substr(0, eq);
          if (keep_params_.contains(decode_uri_component(raw_k))) {
            kept.push_back(pair);
          }
        }
        if (uri->query) {
          xmlFree(uri->query);
          uri->query = nullptr;
        }
        if (uri->query_raw) {
          xmlFree(uri->query_raw);
          uri->query_raw = nullptr;
        }
        if (!kept.empty()) {
          const std::string joined = absl::StrJoin(kept, "&");
          uri->query_raw =
              reinterpret_cast<char *>(xmlCharStrdup(joined.c_str()));
        }
      }
    }

    XmlStringPtr saved(xmlSaveUri(uri.get()));
    if (saved) {
      v = reinterpret_cast<const char *>(saved.get());
    }
  }
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

#include "query/json_query.hpp"
#include "common/hex_utils.hpp"
#include "common/string_utils.hpp"
#include <cstdint>
#include <jsoncons_ext/bson/bson.hpp>
#include <jsoncons_ext/cbor/cbor.hpp>
#include <jsoncons_ext/csv/csv.hpp>
#include <jsoncons_ext/jmespath/jmespath.hpp>
#include <jsoncons_ext/jsonpatch/jsonpatch.hpp>
#include <jsoncons_ext/jsonpath/jsonpath.hpp>
#include <jsoncons_ext/jsonpointer/jsonpointer.hpp>
#include <jsoncons_ext/jsonschema/jsonschema.hpp>
#include <jsoncons_ext/mergepatch/mergepatch.hpp>
#include <jsoncons_ext/msgpack/msgpack.hpp>
#include <jsoncons_ext/ubjson/ubjson.hpp>
#include <stdexcept>
#include <string>
#include <vector>

namespace xsel::query {

namespace {

jsoncons::csv::csv_options make_csv_options(const CsvOptions &opts) {
  jsoncons::csv::csv_options copts;
  copts.assume_header(opts.assume_header)
      .field_delimiter(opts.delimiter)
      .trim(opts.trim);
  return copts;
}

std::vector<uint8_t> from_hex(std::string_view hex) {
  if (hex.size() % 2 != 0) {
    throw std::runtime_error("Invalid hex string: odd length");
  }
  std::vector<uint8_t> out;
  out.reserve(hex.size() / 2);
  for (size_t i = 0; i < hex.size(); i += 2) {
    const int hi = xsel::common::hex_nibble(hex[i]);
    const int lo = xsel::common::hex_nibble(hex[i + 1]);
    if (hi < 0 || lo < 0) {
      throw std::runtime_error("Invalid hex character in binary input");
    }
    out.push_back(static_cast<uint8_t>((hi << 4) | lo));
  }
  return out;
}

std::string to_base64(const std::vector<uint8_t> &bytes) {
  static constexpr char kB64[] =
      "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  std::string out;
  out.reserve(((bytes.size() + 2) / 3) * 4);
  for (size_t i = 0; i < bytes.size(); i += 3) {
    const uint32_t b0 = bytes[i];
    const bool has1 = (i + 1 < bytes.size());
    const bool has2 = (i + 2 < bytes.size());
    const uint32_t b1 = has1 ? bytes[i + 1] : 0;
    const uint32_t b2 = has2 ? bytes[i + 2] : 0;
    const uint32_t triple = (b0 << 16) | (b1 << 8) | b2;
    out.push_back(kB64[(triple >> 18) & 0x3F]);
    out.push_back(kB64[(triple >> 12) & 0x3F]);
    out.push_back(has1 ? kB64[(triple >> 6) & 0x3F] : '=');
    out.push_back(has2 ? kB64[triple & 0x3F] : '=');
  }
  return out;
}

int b64_val(unsigned char c) {
  if (c >= 'A' && c <= 'Z')
    return c - 'A';
  if (c >= 'a' && c <= 'z')
    return 26 + (c - 'a');
  if (c >= '0' && c <= '9')
    return 52 + (c - '0');
  if (c == '+')
    return 62;
  if (c == '/')
    return 63;
  return -1;
}

std::vector<uint8_t> from_base64(std::string_view b64) {
  if (b64.empty())
    return {};
  if (b64.size() % 4 != 0) {
    throw std::runtime_error("Invalid base64 string: length not divisible by 4");
  }
  std::vector<uint8_t> out;
  out.reserve((b64.size() / 4) * 3);
  for (size_t i = 0; i < b64.size(); i += 4) {
    const int v0 = b64_val(static_cast<unsigned char>(b64[i]));
    const int v1 = b64_val(static_cast<unsigned char>(b64[i + 1]));
    const bool pad2 = (b64[i + 2] == '=');
    const bool pad3 = (b64[i + 3] == '=');
    if ((pad2 || pad3) && (i + 4 < b64.size())) {
      throw std::runtime_error(
          "Invalid base64 padding: '=' may only appear at end of input");
    }
    const int v2 = pad2 ? 0 : b64_val(static_cast<unsigned char>(b64[i + 2]));
    const int v3 = pad3 ? 0 : b64_val(static_cast<unsigned char>(b64[i + 3]));
    if (v0 < 0 || v1 < 0 || (!pad2 && v2 < 0) || (!pad3 && v3 < 0) ||
        (pad2 && !pad3)) {
      throw std::runtime_error("Invalid base64 character in binary input");
    }
    const uint32_t triple = (static_cast<uint32_t>(v0) << 18) |
                            (static_cast<uint32_t>(v1) << 12) |
                            (static_cast<uint32_t>(v2) << 6) |
                            static_cast<uint32_t>(v3);
    out.push_back(static_cast<uint8_t>((triple >> 16) & 0xFF));
    if (!pad2) {
      out.push_back(static_cast<uint8_t>((triple >> 8) & 0xFF));
    }
    if (!pad3) {
      out.push_back(static_cast<uint8_t>(triple & 0xFF));
    }
  }
  return out;
}

std::vector<uint8_t> decode_input_bytes(std::string_view input,
                                        BinaryEncoding enc) {
  switch (enc) {
  case BinaryEncoding::Hex:
    return from_hex(input);
  case BinaryEncoding::Base64:
    return from_base64(input);
  case BinaryEncoding::Raw:
    return std::vector<uint8_t>(input.begin(), input.end());
  }
  throw std::runtime_error("Unsupported BinaryEncoding");
}

std::string encode_output_bytes(const std::vector<uint8_t> &bytes,
                                BinaryEncoding enc) {
  switch (enc) {
  case BinaryEncoding::Hex:
    return xsel::common::bytes_to_hex(bytes);
  case BinaryEncoding::Base64:
    return to_base64(bytes);
  case BinaryEncoding::Raw:
    return std::string(bytes.begin(), bytes.end());
  }
  throw std::runtime_error("Unsupported BinaryEncoding");
}

} // namespace

jsoncons::json evaluate_jmespath(const jsoncons::json &doc,
                                 std::string_view expr) {
  try {
    return jsoncons::jmespath::search(doc, expr);
  } catch (const jsoncons::jmespath::jmespath_error &e) {
    throw std::runtime_error(std::string("JMESPath error: ") + e.what());
  }
}

jsoncons::json evaluate_jsonpath(const jsoncons::json &doc,
                                 std::string_view expr) {
  try {
    return jsoncons::jsonpath::json_query(doc, expr);
  } catch (const jsoncons::jsonpath::jsonpath_error &e) {
    throw std::runtime_error(std::string("JSONPath error: ") + e.what());
  }
}

jsoncons::json evaluate(QueryDialect dialect, const jsoncons::json &doc,
                        std::string_view expr) {
  switch (dialect) {
  case QueryDialect::JmesPath:
    return evaluate_jmespath(doc, expr);
  case QueryDialect::JsonPath:
    return evaluate_jsonpath(doc, expr);
  }
  throw std::runtime_error("Unsupported QueryDialect");
}

jsoncons::json parse_json(std::string_view input,
                          const JsonParseOptions &opts) {
  try {
    jsoncons::json_options jopts;
    jopts.max_nesting_depth(opts.max_nesting_depth);
    jopts.lossless_number(opts.lossless_number);
    if (opts.allow_trailing_comma) {
      jopts.err_handler(jsoncons::allow_trailing_commas());
    }
    if (opts.allow_inf_and_nan) {
      jopts.nan_to_num("NaN").inf_to_num("Inf").neginf_to_num("-Inf");
    }
    if (opts.allow_comments) {
      const std::string stripped =
          xsel::common::strip_c_comments(input, true, true, true);
      return jsoncons::json::parse(stripped, jopts);
    }
    return jsoncons::json::parse(input, jopts);
  } catch (const jsoncons::ser_error &e) {
    throw std::runtime_error(std::string("JSON parse error: ") + e.what());
  }
}

std::string dump_json(const jsoncons::json &doc, bool pretty, int indent) {
  std::string out;
  if (pretty) {
    jsoncons::json_options opts;
    opts.indent_size(static_cast<uint8_t>(indent < 0 ? 0 : (indent > 16 ? 16 : indent)));
    jsoncons::encode_json_pretty(doc, out, opts);
  } else {
    jsoncons::encode_json(doc, out);
  }
  return out;
}

jsoncons::json jsonpath_replace(const jsoncons::json &doc,
                                std::string_view expr,
                                const jsoncons::json &replacement) {
  try {
    jsoncons::json copy = doc;
    jsoncons::jsonpath::json_replace(
        copy, expr,
        [&replacement](const std::string &, jsoncons::json &v) {
          v = replacement;
        });
    return copy;
  } catch (const jsoncons::jsonpath::jsonpath_error &e) {
    throw std::runtime_error(std::string("JSONPath replace error: ") + e.what());
  }
}

jsoncons::json json_flatten(const jsoncons::json &doc) {
  try {
    return jsoncons::jsonpath::flatten(doc);
  } catch (const std::exception &e) {
    throw std::runtime_error(std::string("JSON flatten error: ") + e.what());
  }
}

jsoncons::json json_unflatten(const jsoncons::json &doc) {
  try {
    return jsoncons::jsonpath::unflatten(doc);
  } catch (const std::exception &e) {
    throw std::runtime_error(std::string("JSON unflatten error: ") + e.what());
  }
}

jsoncons::json jsonpointer_get(const jsoncons::json &doc,
                               std::string_view pointer) {
  try {
    return jsoncons::jsonpointer::get(doc, pointer);
  } catch (const jsoncons::jsonpointer::jsonpointer_error &e) {
    throw std::runtime_error(std::string("JSONPointer get error: ") + e.what());
  }
}

jsoncons::json jsonpointer_set(const jsoncons::json &doc,
                               std::string_view pointer,
                               const jsoncons::json &value) {
  try {
    jsoncons::json copy = doc;
    jsoncons::jsonpointer::add(copy, pointer, value, true);
    return copy;
  } catch (const jsoncons::jsonpointer::jsonpointer_error &e) {
    throw std::runtime_error(std::string("JSONPointer set error: ") + e.what());
  }
}

jsoncons::json jsonpointer_remove(const jsoncons::json &doc,
                                  std::string_view pointer) {
  try {
    jsoncons::json copy = doc;
    jsoncons::jsonpointer::remove(copy, pointer);
    return copy;
  } catch (const jsoncons::jsonpointer::jsonpointer_error &e) {
    throw std::runtime_error(std::string("JSONPointer remove error: ") +
                             e.what());
  }
}

jsoncons::json apply_json_patch(const jsoncons::json &doc,
                                const jsoncons::json &patch) {
  try {
    jsoncons::json copy = doc;
    jsoncons::jsonpatch::apply_patch(copy, patch);
    return copy;
  } catch (const jsoncons::jsonpatch::jsonpatch_error &e) {
    throw std::runtime_error(std::string("JSON Patch error: ") + e.what());
  }
}

jsoncons::json apply_merge_patch(const jsoncons::json &doc,
                                 const jsoncons::json &patch) {
  try {
    jsoncons::json copy = doc;
    jsoncons::mergepatch::apply_merge_patch(copy, patch);
    return copy;
  } catch (const std::exception &e) {
    throw std::runtime_error(std::string("JSON Merge Patch error: ") +
                             e.what());
  }
}

bool validate_json_schema(const jsoncons::json &doc,
                          const jsoncons::json &schema,
                          std::vector<std::string> *errors_out) {
  try {
    auto compiled = jsoncons::jsonschema::make_json_schema(schema);
    bool valid = true;
    auto reporter = [&](const jsoncons::jsonschema::validation_message &msg) {
      valid = false;
      if (errors_out) {
        errors_out->push_back(msg.instance_location().string() + ": " +
                              msg.message());
      }
      return jsoncons::jsonschema::walk_result::advance;
    };
    compiled.validate(doc, reporter);
    return valid;
  } catch (const jsoncons::jsonschema::schema_error &e) {
    throw std::runtime_error(std::string("JSON Schema error: ") + e.what());
  }
}

jsoncons::json decode_csv(std::string_view input, const CsvOptions &opts) {
  try {
    return jsoncons::csv::decode_csv<jsoncons::json>(input,
                                                     make_csv_options(opts));
  } catch (const jsoncons::ser_error &e) {
    throw std::runtime_error(std::string("CSV decode error: ") + e.what());
  }
}

std::string encode_csv(const jsoncons::json &doc, const CsvOptions &opts) {
  try {
    std::string out;
    jsoncons::csv::encode_csv(doc, out, make_csv_options(opts));
    return out;
  } catch (const std::exception &e) {
    throw std::runtime_error(std::string("CSV encode error: ") + e.what());
  }
}

jsoncons::json decode_binary(BinaryFormat fmt, std::string_view input,
                             BinaryEncoding enc) {
  const auto bytes = decode_input_bytes(input, enc);
  try {
    switch (fmt) {
    case BinaryFormat::Cbor:
      return jsoncons::cbor::decode_cbor<jsoncons::json>(bytes);
    case BinaryFormat::MsgPack:
      return jsoncons::msgpack::decode_msgpack<jsoncons::json>(bytes);
    case BinaryFormat::Bson:
      return jsoncons::bson::decode_bson<jsoncons::json>(bytes);
    case BinaryFormat::Ubjson:
      return jsoncons::ubjson::decode_ubjson<jsoncons::json>(bytes);
    }
  } catch (const jsoncons::ser_error &e) {
    throw std::runtime_error(std::string("Binary decode error: ") + e.what());
  }
  throw std::runtime_error("Unsupported BinaryFormat");
}

std::string encode_binary(BinaryFormat fmt, const jsoncons::json &doc,
                          BinaryEncoding enc) {
  std::vector<uint8_t> bytes;
  try {
    switch (fmt) {
    case BinaryFormat::Cbor:
      jsoncons::cbor::encode_cbor(doc, bytes);
      break;
    case BinaryFormat::MsgPack:
      jsoncons::msgpack::encode_msgpack(doc, bytes);
      break;
    case BinaryFormat::Bson:
      jsoncons::bson::encode_bson(doc, bytes);
      break;
    case BinaryFormat::Ubjson:
      jsoncons::ubjson::encode_ubjson(doc, bytes);
      break;
    }
  } catch (const std::exception &e) {
    throw std::runtime_error(std::string("Binary encode error: ") + e.what());
  }
  return encode_output_bytes(bytes, enc);
}

} // namespace xsel::query

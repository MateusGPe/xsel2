#pragma once

#include "common/json_utils.hpp"
#include <cstdint>
#include <jsoncons/json.hpp>
#include <pybind11/pybind11.h>
#include <stdexcept>
#include <string>

namespace py = pybind11;

namespace xsel::python {

[[nodiscard]] inline py::object json_to_python(const jsoncons::json &val,
                                               int depth = 0) {
  if (depth > xsel::common::MAX_JSON_DEPTH) {
    throw std::runtime_error(
        "Maximum JSON depth exceeded in json_to_python");
  }

  if (val.is_null()) {
    return py::none();
  }
  if (val.is_bool()) {
    return py::bool_(val.as<bool>());
  }
  if (val.is_int64()) {
    return py::int_(val.as<int64_t>());
  }
  if (val.is_uint64()) {
    return py::int_(val.as<uint64_t>());
  }
  if (val.is_double()) {
    return py::float_(val.as<double>());
  }
  if (val.is_string()) {
    return py::str(val.as<std::string>());
  }
  if (val.is_array()) {
    py::list list;
    for (const auto &el : val.array_range()) {
      list.append(json_to_python(el, depth + 1));
    }
    return list;
  }
  if (val.is_object()) {
    py::dict dict;
    for (const auto &kv : val.object_range()) {
      dict[py::str(std::string(kv.key()))] =
          json_to_python(kv.value(), depth + 1);
    }
    return dict;
  }
  return py::none();
}

[[nodiscard]] inline jsoncons::json python_to_json(const py::handle &obj,
                                                   int depth = 0) {
  if (depth > xsel::common::MAX_JSON_DEPTH) {
    throw std::runtime_error(
        "Maximum JSON depth exceeded in python_to_json");
  }

  if (obj.is_none()) {
    return jsoncons::json::null();
  }
  // Check bool BEFORE int, because Python's bool is a subclass of int.
  if (py::isinstance<py::bool_>(obj)) {
    return jsoncons::json(obj.cast<bool>());
  }
  if (py::isinstance<py::int_>(obj)) {
    return jsoncons::json(obj.cast<int64_t>());
  }
  if (py::isinstance<py::float_>(obj)) {
    return jsoncons::json(obj.cast<double>());
  }
  if (py::isinstance<py::str>(obj)) {
    return jsoncons::json(obj.cast<std::string>());
  }
  if (py::isinstance<py::list>(obj) || py::isinstance<py::tuple>(obj)) {
    jsoncons::json arr = jsoncons::json::array();
    for (const auto &item : obj) {
      arr.push_back(python_to_json(item, depth + 1));
    }
    return arr;
  }
  if (py::isinstance<py::dict>(obj)) {
    jsoncons::json out = jsoncons::json::object();
    for (const auto &item : obj.cast<py::dict>()) {
      out.insert_or_assign(py::str(item.first).cast<std::string>(),
                           python_to_json(item.second, depth + 1));
    }
    return out;
  }
  throw std::runtime_error(
      "Unsupported Python type for conversion to jsoncons::json: " +
      std::string(py::str(py::type::of(obj))));
}

} // namespace xsel::python

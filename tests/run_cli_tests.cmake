if(NOT DEFINED XSEL_BIN OR NOT DEFINED SOURCE_DIR)
  message(FATAL_ERROR "XSEL_BIN and SOURCE_DIR must be defined")
endif()

# 1. xsel2 -h and --help
foreach(help_flag -h --help)
  execute_process(
    COMMAND "${XSEL_BIN}" ${help_flag}
    RESULT_VARIABLE rc
    OUTPUT_VARIABLE out
    ERROR_VARIABLE err
  )
  if(NOT rc EQUAL 0)
    message(FATAL_ERROR "xsel2 ${help_flag} failed with ${rc}: ${err}")
  endif()
  if(NOT out MATCHES "xsel2")
    message(FATAL_ERROR "Unexpected ${help_flag} output: ${out}")
  endif()
endforeach()

# 2. xsel2 -s / --selector with inline --html (compact & pretty)
execute_process(
  COMMAND "${XSEL_BIN}" -s "h1.title" --html "<h1 class='title'>  Hello   World </h1>" -c
  RESULT_VARIABLE rc
  OUTPUT_VARIABLE out
  ERROR_VARIABLE err
)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "xsel2 -s failed: ${err}")
endif()
string(STRIP "${out}" out_stripped)
if(NOT out_stripped STREQUAL "{\"matches\":{\"text\":\"Hello World\"}}")
  message(FATAL_ERROR "Unexpected -s output: '${out_stripped}'")
endif()

execute_process(
  COMMAND "${XSEL_BIN}" --selector "h1.title" --html "<h1 class='title'>Pretty</h1>"
  RESULT_VARIABLE rc
  OUTPUT_VARIABLE out
  ERROR_VARIABLE err
)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "xsel2 --selector pretty failed: ${err}")
endif()
if(NOT out MATCHES "\n" OR NOT out MATCHES "\"text\": \"Pretty\"")
  message(FATAL_ERROR "Expected pretty-printed JSON from --selector, got: ${out}")
endif()

# 3. xsel2 --dsl with example files (single scraper / all scrapers when -n omitted)
execute_process(
  COMMAND "${XSEL_BIN}" --dsl "${SOURCE_DIR}/examples/product.dsl" --html "${SOURCE_DIR}/examples/sample.html" --compact
  RESULT_VARIABLE rc
  OUTPUT_VARIABLE out
  ERROR_VARIABLE err
)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "xsel2 --dsl failed: ${err}")
endif()
if(NOT out MATCHES "Mechanical Keyboard Pro")
  message(FATAL_ERROR "Expected 'Mechanical Keyboard Pro' in DSL output, got: ${out}")
endif()

# 3b. xsel2 -d without -n runs all scrapers in metadata.dsl
execute_process(
  COMMAND "${XSEL_BIN}" -d "${SOURCE_DIR}/examples/metadata.dsl" --html "${SOURCE_DIR}/examples/metadata_sample.html" -c
  RESULT_VARIABLE rc
  OUTPUT_VARIABLE out
  ERROR_VARIABLE err
)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "xsel2 metadata.dsl failed: ${err}")
endif()
if(NOT out MATCHES "\"open_graph\":" OR NOT out MATCHES "\"metadata_summary\":")
  message(FATAL_ERROR "Expected open_graph and metadata_summary in metadata.dsl output, got: ${out}")
endif()

# 3c. xsel2 -d with -n on marketplace.dsl
execute_process(
  COMMAND "${XSEL_BIN}" -d "${SOURCE_DIR}/examples/marketplace.dsl" --name catalog_summary --html "${SOURCE_DIR}/examples/marketplace_sample.html" -c
  RESULT_VARIABLE rc
  OUTPUT_VARIABLE out
  ERROR_VARIABLE err
)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "xsel2 marketplace.dsl failed: ${err}")
endif()
if(NOT out MATCHES "\"warehouse\":\"US-EAST\"" OR NOT out MATCHES "\"api_base\":\"https://cdn.example.com/catalog/v2\"")
  message(FATAL_ERROR "Expected warehouse and api_base in marketplace.dsl output, got: ${out}")
endif()

# 3d. xsel2 -d on all_steps_showcase.dsl
execute_process(
  COMMAND "${XSEL_BIN}" -d "${SOURCE_DIR}/examples/all_steps_showcase.dsl" -n urls_dates_unicode --html "${SOURCE_DIR}/examples/all_steps_showcase.html" -c
  RESULT_VARIABLE rc
  OUTPUT_VARIABLE out
  ERROR_VARIABLE err
)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "xsel2 all_steps_showcase.dsl failed: ${err}")
endif()
if(NOT out MATCHES "\"slug\":\"cafe-resume-mechanical-gear\"")
  message(FATAL_ERROR "Expected slug in all_steps_showcase.dsl output, got: ${out}")
endif()

# 4. Pipe mode: implicit stdin when --html is omitted
execute_process(
  COMMAND "${XSEL_BIN}" -d "${SOURCE_DIR}/examples/product.dsl" -c
  INPUT_FILE "${SOURCE_DIR}/examples/sample.html"
  RESULT_VARIABLE rc
  OUTPUT_VARIABLE out
  ERROR_VARIABLE err
)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "xsel2 implicit stdin failed: ${err}")
endif()
if(NOT out MATCHES "Mechanical Keyboard Pro")
  message(FATAL_ERROR "Expected 'Mechanical Keyboard Pro' in pipe output, got: ${out}")
endif()

# 5. Explicit --html - for stdin
execute_process(
  COMMAND "${XSEL_BIN}" -s "h1" --html - -c
  INPUT_FILE "${SOURCE_DIR}/examples/sample.html"
  RESULT_VARIABLE rc
  OUTPUT_VARIABLE out
  ERROR_VARIABLE err
)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "xsel2 -s --html - failed: ${err}")
endif()
if(NOT out MATCHES "Mechanical Keyboard Pro")
  message(FATAL_ERROR "Expected 'Mechanical Keyboard Pro' from stdin HTML, got: ${out}")
endif()

# 6. Batch mode: --ndjson (default) with multi-threading (-j 2)
execute_process(
  COMMAND "${XSEL_BIN}" -d "${SOURCE_DIR}/examples/product.dsl" -n summary
          --batch "${SOURCE_DIR}/examples/sample.html" "${SOURCE_DIR}/examples/metadata_sample.html"
          -j 2
  RESULT_VARIABLE rc
  OUTPUT_VARIABLE out
  ERROR_VARIABLE err
)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "xsel2 --batch --ndjson failed: ${err}")
endif()
string(STRIP "${out}" out_stripped)
string(REPLACE "\n" ";" ndjson_lines "${out_stripped}")
list(LENGTH ndjson_lines line_count)
if(NOT line_count EQUAL 2)
  message(FATAL_ERROR "Expected 2 NDJSON lines, got ${line_count}: ${out}")
endif()
if(NOT out MATCHES "Mechanical Keyboard Pro")
  message(FATAL_ERROR "Expected Mechanical Keyboard Pro in NDJSON output: ${out}")
endif()

# 7. Batch mode: --json-array with -j 0 (auto threads)
execute_process(
  COMMAND "${XSEL_BIN}" -s "h1"
          --batch "${SOURCE_DIR}/examples/sample.html" "${SOURCE_DIR}/examples/marketplace_sample.html"
          --json-array -j 0 -c
  RESULT_VARIABLE rc
  OUTPUT_VARIABLE out
  ERROR_VARIABLE err
)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "xsel2 --batch --json-array failed: ${err}")
endif()
if(NOT out MATCHES "^\\[" OR NOT out MATCHES "\"result\":")
  message(FATAL_ERROR "Unexpected --json-array output: ${out}")
endif()

# 8. Batch mode: --json-object
execute_process(
  COMMAND "${XSEL_BIN}" -s "h1"
          --batch "${SOURCE_DIR}/examples/sample.html" "${SOURCE_DIR}/examples/marketplace_sample.html"
          --json-object -c
  RESULT_VARIABLE rc
  OUTPUT_VARIABLE out
  ERROR_VARIABLE err
)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "xsel2 --batch --json-object failed: ${err}")
endif()
if(NOT out MATCHES "^\\{" OR NOT out MATCHES "sample.html")
  message(FATAL_ERROR "Unexpected --json-object output: ${out}")
endif()

# 9. Batch mode: --out-dir
set(batch_out_dir "${CMAKE_CURRENT_BINARY_DIR}/cli_batch_out_test")
file(REMOVE_RECURSE "${batch_out_dir}")
execute_process(
  COMMAND "${XSEL_BIN}" -d "${SOURCE_DIR}/examples/product.dsl" -n summary
          --batch "${SOURCE_DIR}/examples/sample.html" "${SOURCE_DIR}/examples/metadata_sample.html"
          --out-dir "${batch_out_dir}" -j 2 -c
  RESULT_VARIABLE rc
  OUTPUT_VARIABLE out
  ERROR_VARIABLE err
)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "xsel2 --batch --out-dir failed: ${err}")
endif()
if(NOT EXISTS "${batch_out_dir}/sample.html.json" OR NOT EXISTS "${batch_out_dir}/metadata_sample.html.json")
  message(FATAL_ERROR "Expected per-file JSON outputs in ${batch_out_dir}")
endif()
file(READ "${batch_out_dir}/sample.html.json" sample_out_json)
if(NOT sample_out_json MATCHES "Mechanical Keyboard Pro")
  message(FATAL_ERROR "Unexpected sample.html.json content: ${sample_out_json}")
endif()
file(REMOVE_RECURSE "${batch_out_dir}")

# 10. Negative & edge-case CLI tests
foreach(bad_cmd
  ""
  "--html;<p>hi</p>"
  "-d;${SOURCE_DIR}/examples/product.dsl;-s;h1;--html;<p>hi</p>"
  "-s;h1;--html;<p>hi</p>;--bogus"
  "-s"
  "-d;${SOURCE_DIR}/examples/does_not_exist.dsl;--html;<p>hi</p>"
  "-d;${SOURCE_DIR}/examples/product.dsl;-n;nonexistent;--html;<p>hi</p>"
  "-s;h1;--html;<p>hi</p>;--batch;${SOURCE_DIR}/examples/sample.html"
  "-s;h1;--batch"
  "-s;h1;--batch;${SOURCE_DIR}/examples/sample.html;--ndjson;--json-array"
  "-s;h1;--batch;${SOURCE_DIR}/examples/sample.html;-j;-1"
  "-s;h1;--json-array"
  "ahocorasick;--patterns;foo;--text;foo"
  "jmespath;--expr;@;--json;{}"
  "jsonpath;--expr;$;--json;{}"
)
  if(bad_cmd STREQUAL "")
    execute_process(
      COMMAND "${XSEL_BIN}"
      RESULT_VARIABLE rc
      OUTPUT_QUIET
      ERROR_QUIET
    )
  else()
    execute_process(
      COMMAND "${XSEL_BIN}" ${bad_cmd}
      RESULT_VARIABLE rc
      OUTPUT_QUIET
      ERROR_QUIET
    )
  endif()
  if(rc EQUAL 0)
    message(FATAL_ERROR "Expected non-zero exit code for invalid CLI args: '${bad_cmd}'")
  endif()
endforeach()

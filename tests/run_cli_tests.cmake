if(NOT DEFINED CLI_BIN OR NOT DEFINED PIPE_BIN OR NOT DEFINED SOURCE_DIR)
  message(FATAL_ERROR "CLI_BIN, PIPE_BIN, and SOURCE_DIR must be defined")
endif()

# 1. xmlsel-cli --help
execute_process(
  COMMAND "${CLI_BIN}" --help
  RESULT_VARIABLE rc
  OUTPUT_VARIABLE out
  ERROR_VARIABLE err
)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "xmlsel-cli --help failed with ${rc}: ${err}")
endif()
if(NOT out MATCHES "xmlsel-cli")
  message(FATAL_ERROR "Unexpected --help output: ${out}")
endif()

# 2. xmlsel-cli scraper --selector
execute_process(
  COMMAND "${CLI_BIN}" scraper --selector "h1.title" --html "<h1 class='title'>  Hello   World </h1>" --compact
  RESULT_VARIABLE rc
  OUTPUT_VARIABLE out
  ERROR_VARIABLE err
)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "xmlsel-cli scraper --selector failed: ${err}")
endif()
string(STRIP "${out}" out_stripped)
if(NOT out_stripped STREQUAL "{\"matches\":{\"text\":\"Hello World\"}}")
  message(FATAL_ERROR "Unexpected scraper --selector output: '${out_stripped}'")
endif()

# 3. xmlsel-cli scraper --dsl with example files
execute_process(
  COMMAND "${CLI_BIN}" scraper --dsl "${SOURCE_DIR}/examples/product.dsl" --html "${SOURCE_DIR}/examples/sample.html" --compact
  RESULT_VARIABLE rc
  OUTPUT_VARIABLE out
  ERROR_VARIABLE err
)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "xmlsel-cli scraper --dsl failed: ${err}")
endif()
if(NOT out MATCHES "Mechanical Keyboard Pro")
  message(FATAL_ERROR "Expected 'Mechanical Keyboard Pro' in DSL output, got: ${out}")
endif()

# 3b. xmlsel-cli scraper --dsl with exhaustive metadata example
execute_process(
  COMMAND "${CLI_BIN}" scraper --dsl "${SOURCE_DIR}/examples/metadata.dsl" --html "${SOURCE_DIR}/examples/metadata_sample.html" --compact
  RESULT_VARIABLE rc
  OUTPUT_VARIABLE out
  ERROR_VARIABLE err
)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "xmlsel-cli metadata.dsl failed: ${err}")
endif()
if(NOT out MATCHES "\"open_graph\":" OR NOT out MATCHES "\"metadata_summary\":")
  message(FATAL_ERROR "Expected open_graph and metadata_summary in metadata.dsl output, got: ${out}")
endif()

# 3c. xmlsel-cli scraper --dsl with comprehensive marketplace example
execute_process(
  COMMAND "${CLI_BIN}" scraper --dsl "${SOURCE_DIR}/examples/marketplace.dsl" --name catalog_summary --html "${SOURCE_DIR}/examples/marketplace_sample.html" --compact
  RESULT_VARIABLE rc
  OUTPUT_VARIABLE out
  ERROR_VARIABLE err
)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "xmlsel-cli marketplace.dsl failed: ${err}")
endif()
if(NOT out MATCHES "\"warehouse\":\"US-EAST\"" OR NOT out MATCHES "\"api_base\":\"https://cdn.example.com/catalog/v2\"")
  message(FATAL_ERROR "Expected warehouse and api_base in marketplace.dsl output, got: ${out}")
endif()

# 4. xmlsel-cli ahocorasick
execute_process(
  COMMAND "${CLI_BIN}" ahocorasick --patterns "foo,bar" --text "xxfooyybarzz"
  RESULT_VARIABLE rc
  OUTPUT_VARIABLE out
  ERROR_VARIABLE err
)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "xmlsel-cli ahocorasick failed: ${err}")
endif()
if(NOT out MATCHES "\"match\": \"foo\"" OR NOT out MATCHES "\"match\": \"bar\"")
  message(FATAL_ERROR "Unexpected ahocorasick output: ${out}")
endif()

# 5. xmlsel-cli jmespath and jsonpath
execute_process(
  COMMAND "${CLI_BIN}" jmespath --expr "items[?price > \`10\`].name" --json "{\"items\":[{\"name\":\"a\",\"price\":5},{\"name\":\"b\",\"price\":20}]}" --compact
  RESULT_VARIABLE rc
  OUTPUT_VARIABLE out
  ERROR_VARIABLE err
)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "xmlsel-cli jmespath failed: ${err}")
endif()
string(STRIP "${out}" out_stripped)
if(NOT out_stripped STREQUAL "[\"b\"]")
  message(FATAL_ERROR "Unexpected jmespath output: '${out_stripped}'")
endif()

execute_process(
  COMMAND "${CLI_BIN}" jsonpath --expr "$.items[*].name" --json "{\"items\":[{\"name\":\"a\"},{\"name\":\"b\"}]}" --compact
  RESULT_VARIABLE rc
  OUTPUT_VARIABLE out
  ERROR_VARIABLE err
)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "xmlsel-cli jsonpath failed: ${err}")
endif()
string(STRIP "${out}" out_stripped)
if(NOT out_stripped STREQUAL "[\"a\",\"b\"]")
  message(FATAL_ERROR "Unexpected jsonpath output: '${out_stripped}'")
endif()

# 6. xmlsel-cli error cases must return non-zero
execute_process(
  COMMAND "${CLI_BIN}" unknown_cmd
  RESULT_VARIABLE rc
  OUTPUT_QUIET
  ERROR_QUIET
)
if(rc EQUAL 0)
  message(FATAL_ERROR "Expected non-zero exit code for unknown subcommand")
endif()

# 7. xmlsel-pipe reading HTML from stdin
execute_process(
  COMMAND "${PIPE_BIN}" -d "${SOURCE_DIR}/examples/product.dsl" -c
  INPUT_FILE "${SOURCE_DIR}/examples/sample.html"
  RESULT_VARIABLE rc
  OUTPUT_VARIABLE out
  ERROR_VARIABLE err
)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "xmlsel-pipe failed: ${err}")
endif()
if(NOT out MATCHES "Mechanical Keyboard Pro")
  message(FATAL_ERROR "Expected 'Mechanical Keyboard Pro' in xmlsel-pipe output, got: ${out}")
endif()

# 8. xmlsel-pipe missing -d must fail
execute_process(
  COMMAND "${PIPE_BIN}"
  INPUT_FILE "${SOURCE_DIR}/examples/sample.html"
  RESULT_VARIABLE rc
  OUTPUT_QUIET
  ERROR_QUIET
)
if(rc EQUAL 0)
  message(FATAL_ERROR "Expected non-zero exit code when -d is omitted in xmlsel-pipe")
endif()

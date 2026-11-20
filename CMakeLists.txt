cmake_minimum_required(VERSION 3.24)

project(
  xsel2
  VERSION 0.4.0
  DESCRIPTION "High-performance HTML/XML scraping, multi-pattern text search, and JSON transformation engine"
  LANGUAGES C CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(GNUInstallDirs)
include(CTest)
include(cmake/CPM.cmake)

if(DEFINED SCRAPERPY_VERSION_INFO)
  message(STATUS "Project version (from setup.py): ${SCRAPERPY_VERSION_INFO}")
endif()

# ── Dependencies ─────────────────────────────────────────────────────────────
set(XSEL_FETCH_LIBXML2 "AUTO" CACHE STRING
  "Whether to fetch libxml2 from source: AUTO (fallback if not found), ON (always fetch), OFF (require system)")
set_property(CACHE XSEL_FETCH_LIBXML2 PROPERTY STRINGS AUTO ON OFF)

if(XSEL_FETCH_LIBXML2 STREQUAL "OFF")
  find_package(LibXml2 REQUIRED)
else()
  if(XSEL_FETCH_LIBXML2 STREQUAL "AUTO" AND NOT CPM_DOWNLOAD_ALL AND NOT CPM_DOWNLOAD_LibXml2)
    find_package(LibXml2 QUIET)
  endif()

  if(NOT LibXml2_FOUND)
    message(STATUS "xsel2: LibXml2 not found on system; fetching GNOME/libxml2 via CPM")
    CPMAddPackage(
      NAME LibXml2
      GITHUB_REPOSITORY GNOME/libxml2
      GIT_TAG v2.13.9
      GIT_SHALLOW TRUE
      OPTIONS
        "BUILD_SHARED_LIBS OFF"
        "LIBXML2_WITH_ICONV OFF"
        "LIBXML2_WITH_ICU OFF"
        "LIBXML2_WITH_LZMA OFF"
        "LIBXML2_WITH_ZLIB OFF"
        "LIBXML2_WITH_MODULES OFF"
        "LIBXML2_WITH_PYTHON OFF"
        "LIBXML2_WITH_PROGRAMS OFF"
        "LIBXML2_WITH_TESTS OFF")
  endif()
endif()

CPMAddPackage(
  NAME jsoncons
  GITHUB_REPOSITORY danielaparker/jsoncons
  GIT_TAG v1.0.0
  GIT_SHALLOW TRUE
  OPTIONS
    "JSONCONS_BUILD_TESTS OFF")

CPMAddPackage(
  NAME pybind11
  GITHUB_REPOSITORY pybind/pybind11
  GIT_TAG stable
  GIT_SHALLOW TRUE)

CPMAddPackage(
  NAME lxml2_query
  GITHUB_REPOSITORY MateusGPe/lxml2-query-v1
  GIT_TAG main
  GIT_SHALLOW TRUE)

if(TARGET lxml2-query)
  # GCC 15+ no longer transitively provides <cstdint> in TagSelector.cpp.
  target_compile_options(lxml2-query PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-include cstdint>)
endif()

CPMAddPackage(
  NAME absl
  GITHUB_REPOSITORY abseil/abseil-cpp
  GIT_TAG lts_2025_08_14
  GIT_SHALLOW TRUE
  OPTIONS
    "ABSL_PROPAGATE_CXX_STD ON"
    "ABSL_BUILD_TESTING OFF"
    "ABSL_BUILD_TEST_HELPERS OFF"
    "ABSL_ENABLE_INSTALL OFF")

CPMAddPackage(
  NAME re2
  GITHUB_REPOSITORY google/re2
  GIT_TAG 2025-11-05
  GIT_SHALLOW TRUE
  OPTIONS
    "RE2_BUILD_TESTING OFF"
    "RE2_INSTALL OFF")

CPMAddPackage(
  NAME utf8proc
  GITHUB_REPOSITORY JuliaStrings/utf8proc
  GIT_TAG v2.10.0
  GIT_SHALLOW TRUE
  OPTIONS
    "BUILD_SHARED_LIBS OFF"
    "UTF8PROC_ENABLE_TESTING OFF")

if(BUILD_TESTING AND NOT DEFINED SCRAPERPY_VERSION_INFO)
  CPMAddPackage(
    NAME Catch2
    GITHUB_REPOSITORY catchorg/Catch2
    GIT_TAG v3.7.1
    GIT_SHALLOW TRUE)
endif()

set(XSEL_WARN_FLAGS
  $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic>
  $<$<CXX_COMPILER_ID:MSVC>:/W4>
  $<$<CONFIG:Release>:-O3>)

# ── Common library ───────────────────────────────────────────────────────────
add_library(xsel_common STATIC
  src/common/json_utils.cpp)

target_include_directories(xsel_common PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_compile_features(xsel_common PUBLIC cxx_std_20)
target_compile_options(xsel_common PRIVATE ${XSEL_WARN_FLAGS})
target_link_libraries(xsel_common PUBLIC jsoncons)

# ── Digger library ───────────────────────────────────────────────────────────
add_library(digger STATIC
  src/digger/tokenizer.cpp
  src/digger/digger.cpp)

target_include_directories(digger PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_compile_features(digger PUBLIC cxx_std_20)
target_compile_options(digger PRIVATE ${XSEL_WARN_FLAGS})
target_link_libraries(digger PUBLIC xsel_common jsoncons)

# ── Query library (JMESPath + JSONPath) ──────────────────────────────────────
add_library(xsel_query STATIC
  src/query/json_query.cpp)

target_include_directories(xsel_query PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_compile_features(xsel_query PUBLIC cxx_std_20)
target_compile_options(xsel_query PRIVATE ${XSEL_WARN_FLAGS})
target_link_libraries(xsel_query PUBLIC jsoncons)

# ── Scraper library ──────────────────────────────────────────────────────────
add_library(scraper STATIC
  src/scraper/merge.cpp
  src/scraper/extractor.cpp
  src/scraper/scraper.cpp
  src/scraper/dsl/lexer.cpp
  src/scraper/dsl/value.cpp
  src/scraper/dsl/step_registry.cpp
  src/scraper/dsl_parser.cpp
  src/scraper/steps/text_steps.cpp
  src/scraper/steps/regex_steps.cpp
  src/scraper/steps/dom_steps.cpp
  src/scraper/steps/block_steps.cpp
  src/scraper/steps/dict_steps.cpp
  src/scraper/steps/json_steps.cpp
  src/scraper/steps/type_steps.cpp
  src/scraper/steps/url_steps.cpp)

target_include_directories(scraper PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_compile_features(scraper PUBLIC cxx_std_20)
target_compile_options(scraper PRIVATE ${XSEL_WARN_FLAGS})
target_link_libraries(scraper PUBLIC
  xsel_common
  digger
  xsel_query
  jsoncons
  mgp::lxml2-query
  re2::re2
  absl::strings
  absl::flat_hash_map
  absl::flat_hash_set
  absl::time
  utf8proc)

# ── Python module target ─────────────────────────────────────────────────────
pybind11_add_module(_scraperpy src/python/module.cpp)
target_include_directories(_scraperpy PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_compile_features(_scraperpy PRIVATE cxx_std_20)
target_compile_options(_scraperpy PRIVATE ${XSEL_WARN_FLAGS})
target_link_libraries(_scraperpy PRIVATE scraper)

if(DEFINED SCRAPERPY_VERSION_INFO)
  target_compile_definitions(_scraperpy
    PRIVATE SCRAPERPY_VERSION_INFO="${SCRAPERPY_VERSION_INFO}")
endif()

# ── Executables & Tests ──────────────────────────────────────────────────────
if(NOT DEFINED SCRAPERPY_VERSION_INFO)
  add_executable(xmlsel-cli src/cli/main.cpp)
  target_compile_features(xmlsel-cli PRIVATE cxx_std_20)
  target_compile_options(xmlsel-cli PRIVATE ${XSEL_WARN_FLAGS})
  target_link_libraries(xmlsel-cli PRIVATE scraper)

  add_executable(xmlsel-pipe src/cli/main_pipe.cpp)
  target_compile_features(xmlsel-pipe PRIVATE cxx_std_20)
  target_compile_options(xmlsel-pipe PRIVATE ${XSEL_WARN_FLAGS})
  target_link_libraries(xmlsel-pipe PRIVATE scraper)

  if(BUILD_TESTING)
    add_executable(xsel_tests
      tests/test_ahocorasick.cpp
      tests/test_common.cpp
      tests/test_digger.cpp
      tests/test_dsl.cpp
      tests/test_merge.cpp
      tests/test_query.cpp
      tests/test_scraper.cpp
      tests/test_steps.cpp)
    target_compile_features(xsel_tests PRIVATE cxx_std_20)
    target_compile_options(xsel_tests PRIVATE ${XSEL_WARN_FLAGS})
    target_link_libraries(xsel_tests PRIVATE scraper Catch2::Catch2WithMain)

    list(APPEND CMAKE_MODULE_PATH ${Catch2_SOURCE_DIR}/extras)
    include(Catch)
    catch_discover_tests(xsel_tests)

    add_test(
      NAME cli_integration_tests
      COMMAND ${CMAKE_COMMAND}
        -DCLI_BIN=$<TARGET_FILE:xmlsel-cli>
        -DPIPE_BIN=$<TARGET_FILE:xmlsel-pipe>
        -DSOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_cli_tests.cmake)

    find_program(UV_EXECUTABLE uv)
    if(UV_EXECUTABLE AND Python_EXECUTABLE)
      if(WIN32)
        set(_xsel_py_path "$<TARGET_FILE_DIR:_scraperpy>\\;${CMAKE_CURRENT_SOURCE_DIR}")
      else()
        set(_xsel_py_path "$<TARGET_FILE_DIR:_scraperpy>:${CMAKE_CURRENT_SOURCE_DIR}")
      endif()
      add_test(
        NAME python_pytest
        COMMAND ${UV_EXECUTABLE} run --with pytest --python ${Python_EXECUTABLE}
                pytest ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_python.py -v)
      set_tests_properties(python_pytest PROPERTIES
        ENVIRONMENT "PYTHONPATH=${_xsel_py_path}")
    endif()
  endif()

  install(TARGETS _scraperpy LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
  install(TARGETS xmlsel-cli xmlsel-pipe RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()

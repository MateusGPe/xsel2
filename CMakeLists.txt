cmake_minimum_required(VERSION 3.24)

project(
  xsel2
  VERSION 0.6.0
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

if(SKBUILD AND NOT DEFINED SCRAPERPY_VERSION_INFO AND DEFINED SKBUILD_PROJECT_VERSION)
  set(SCRAPERPY_VERSION_INFO "${SKBUILD_PROJECT_VERSION}")
endif()

if(DEFINED SCRAPERPY_VERSION_INFO)
  message(STATUS "Project version (from Python packaging): ${SCRAPERPY_VERSION_INFO}")
endif()

# ── Dependencies ─────────────────────────────────────────────────────────────
option(XSEL_BUILD_WASM "Build xsel2 WebAssembly module" OFF)
option(XSEL_USE_XPATH3 "Enable XPath3Step using lxml2-query XPath 3.1 engine" ON)

# Silence CMake < 3.10 deprecation warnings in third-party dependencies (e.g. jsoncons) on CMake 4+
if(CMAKE_VERSION VERSION_GREATER_EQUAL "4.0" AND NOT DEFINED CMAKE_POLICY_VERSION_MINIMUM)
  set(CMAKE_POLICY_VERSION_MINIMUM 3.10)
endif()

# Silence pybind11 FindPython compatibility mode warning
if(NOT DEFINED PYBIND11_FINDPYTHON OR PYBIND11_FINDPYTHON STREQUAL "COMPAT")
  set(PYBIND11_FINDPYTHON NEW CACHE STRING "Use FindPython for pybind11" FORCE)
endif()

# Suppress compiler warnings inside third-party dependency builds
set(_XSEL_SAVED_C_FLAGS "${CMAKE_C_FLAGS}")
set(_XSEL_SAVED_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
if(MSVC)
  string(APPEND CMAKE_C_FLAGS " /w")
  string(APPEND CMAKE_CXX_FLAGS " /w")
else()
  string(APPEND CMAKE_C_FLAGS " -w")
  string(APPEND CMAKE_CXX_FLAGS " -w")
endif()

if(EMSCRIPTEN)
  set(XSEL_BUILD_WASM ON)
  set(XSEL_FETCH_LIBXML2 "ON" CACHE STRING
    "Whether to fetch libxml2 from source: AUTO (fallback if not found), ON (always fetch), OFF (require system)" FORCE)
  add_compile_options(-fexceptions)
  add_link_options(-fexceptions)
endif()

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
      SYSTEM ON
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
  SYSTEM ON
  OPTIONS
    "JSONCONS_BUILD_TESTS OFF")

if(NOT EMSCRIPTEN)
  find_package(pybind11 CONFIG QUIET)
  if(NOT pybind11_FOUND)
    CPMAddPackage(
      NAME pybind11
      GITHUB_REPOSITORY pybind/pybind11
      GIT_TAG stable
      GIT_SHALLOW TRUE
      SYSTEM ON)
  endif()
endif()

CPMAddPackage(
  NAME absl
  GITHUB_REPOSITORY abseil/abseil-cpp
  GIT_TAG lts_2025_08_14
  GIT_SHALLOW TRUE
  SYSTEM ON
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
  SYSTEM ON
  OPTIONS
    "RE2_BUILD_TESTING OFF"
    "RE2_INSTALL OFF")

CPMAddPackage(
  NAME utf8proc
  GITHUB_REPOSITORY JuliaStrings/utf8proc
  GIT_TAG v2.10.0
  GIT_SHALLOW TRUE
  SYSTEM ON
  OPTIONS
    "BUILD_SHARED_LIBS OFF"
    "UTF8PROC_ENABLE_TESTING OFF")

CPMAddPackage(
  NAME lxml2_query
  GITHUB_REPOSITORY MateusGPe/lxml2-query-v1
  GIT_TAG main
  GIT_SHALLOW TRUE
  SYSTEM ON
  OPTIONS
    "LXML2_QUERY_XPATH3 ${XSEL_USE_XPATH3}"
    "LXML2_QUERY_PUBLIC_XPATH_DEPS ${XSEL_USE_XPATH3}")

if(TARGET lxml2-query)
  # GCC 15+ no longer transitively provides <cstdint> in TagSelector.cpp.
  target_compile_options(lxml2-query PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-include cstdint -w>
    $<$<CXX_COMPILER_ID:MSVC>:/w>)
endif()

if(BUILD_TESTING AND NOT DEFINED SCRAPERPY_VERSION_INFO)
  CPMAddPackage(
    NAME Catch2
    GITHUB_REPOSITORY catchorg/Catch2
    GIT_TAG v3.7.1
    GIT_SHALLOW TRUE
    SYSTEM ON)
endif()

set(CMAKE_C_FLAGS "${_XSEL_SAVED_C_FLAGS}")
set(CMAKE_CXX_FLAGS "${_XSEL_SAVED_CXX_FLAGS}")

set(XSEL_WARN_FLAGS
  $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic>
  $<$<CXX_COMPILER_ID:MSVC>:/W4 /utf-8>
  $<$<AND:$<CONFIG:Release>,$<CXX_COMPILER_ID:GNU,Clang>>:-O3>)

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
if(XSEL_USE_XPATH3)
  target_compile_definitions(scraper PUBLIC XSEL_USE_XPATH3)
endif()
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
if(NOT EMSCRIPTEN)
  pybind11_add_module(_scraperpy src/python/module.cpp)
  target_include_directories(_scraperpy PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
  target_compile_features(_scraperpy PRIVATE cxx_std_20)
  target_compile_options(_scraperpy PRIVATE ${XSEL_WARN_FLAGS})
  target_link_libraries(_scraperpy PRIVATE scraper)

  if(DEFINED SCRAPERPY_VERSION_INFO)
    target_compile_definitions(_scraperpy
      PRIVATE SCRAPERPY_VERSION_INFO="${SCRAPERPY_VERSION_INFO}")
  endif()

  if(SKBUILD)
    install(TARGETS _scraperpy LIBRARY DESTINATION scraperpy)
    install(FILES scraperpy-stubs/_scraperpy.pyi DESTINATION scraperpy)
  endif()
endif()

# ── WebAssembly target ───────────────────────────────────────────────────────
if(XSEL_BUILD_WASM AND EMSCRIPTEN)
  add_executable(xsel2_wasm src/wasm/wasm_entry.cpp)
  target_compile_features(xsel2_wasm PRIVATE cxx_std_20)
  target_compile_options(xsel2_wasm PRIVATE ${XSEL_WARN_FLAGS})
  target_link_libraries(xsel2_wasm PRIVATE scraper)
  set_target_properties(xsel2_wasm PROPERTIES
    OUTPUT_NAME "xsel2"
    SUFFIX ".js")
  target_link_options(xsel2_wasm PRIVATE
    "--bind"
    "-sMODULARIZE=1"
    "-sEXPORT_ES6=1"
    "-sEXPORT_NAME=createXsel2Module"
    "-sENVIRONMENT=web,worker,node"
    "-sALLOW_MEMORY_GROWTH=1"
    "-sINITIAL_MEMORY=33554432")
endif()

# ── Executables & Tests ──────────────────────────────────────────────────────
if(NOT DEFINED SCRAPERPY_VERSION_INFO AND NOT EMSCRIPTEN)
  find_package(Threads REQUIRED)

  add_executable(xsel2 src/cli/main.cpp)
  target_compile_features(xsel2 PRIVATE cxx_std_20)
  target_compile_options(xsel2 PRIVATE ${XSEL_WARN_FLAGS})
  target_link_libraries(xsel2 PRIVATE scraper Threads::Threads)

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
        -DXSEL_BIN=$<TARGET_FILE:xsel2>
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

      add_test(
        NAME cli_pytest
        COMMAND ${UV_EXECUTABLE} run --with pytest --python ${Python_EXECUTABLE}
                pytest ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_cli.py -v)
      set_tests_properties(cli_pytest PROPERTIES
        ENVIRONMENT "XSEL_BIN=$<TARGET_FILE:xsel2>;XSEL_SOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}")
    endif()
  endif()

  install(TARGETS _scraperpy LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
  install(TARGETS xsel2 RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()

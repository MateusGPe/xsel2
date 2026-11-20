#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PRESET="debug"
CLEAN=0
JOBS="$(nproc 2>/dev/null || echo 4)"
FILTER=""

usage() {
  cat <<'EOF'
Usage: scripts/build_and_test.sh [options]

Options:
  -p, --preset <name>   CMake preset: debug (default), release, asan-ubsan
  -c, --clean           Clean build directory before configuring
  -R, --filter <regex>  Run only CTest tests matching regex
  -j, --jobs <N>        Parallel build/test jobs (default: nproc)
  -h, --help            Show this help message
EOF
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    -p|--preset)
      PRESET="${2:?Missing value for $1}"
      shift 2
      ;;
    -c|--clean)
      CLEAN=1
      shift
      ;;
    -R|--filter)
      FILTER="${2:?Missing value for $1}"
      shift 2
      ;;
    -j|--jobs)
      JOBS="${2:?Missing value for $1}"
      shift 2
      ;;
    -h|--help)
      usage
      exit 0
      ;;
    *)
      echo "Unknown option: $1" >&2
      usage >&2
      exit 1
      ;;
  esac
done

BUILD_ARGS=(--preset "$PRESET" --jobs "$JOBS")
if [[ "$CLEAN" -eq 1 ]]; then
  BUILD_ARGS+=(--clean)
fi

TEST_ARGS=(--preset "$PRESET" --jobs "$JOBS")
if [[ -n "$FILTER" ]]; then
  TEST_ARGS+=(-R "$FILTER")
fi

"$ROOT_DIR/scripts/build.sh" "${BUILD_ARGS[@]}"
"$ROOT_DIR/scripts/test.sh" "${TEST_ARGS[@]}"

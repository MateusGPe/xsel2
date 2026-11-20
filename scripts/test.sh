#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PRESET="debug"
JOBS="$(nproc 2>/dev/null || echo 4)"
FILTER=""
BUILD_FIRST=0

usage() {
  cat <<'EOF'
Usage: scripts/test.sh [options]

Options:
  -p, --preset <name>   CMake preset to test: debug (default), release, asan-ubsan
  -b, --build           Build before running tests
  -R, --filter <regex>  Run only CTest tests matching regex
  -j, --jobs <N>        Parallel test jobs (default: nproc)
  -h, --help            Show this help message
EOF
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    -p|--preset)
      PRESET="${2:?Missing value for $1}"
      shift 2
      ;;
    -b|--build)
      BUILD_FIRST=1
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

case "$PRESET" in
  debug)      BUILD_DIR="$ROOT_DIR/build" ;;
  release)    BUILD_DIR="$ROOT_DIR/build-release" ;;
  asan-ubsan) BUILD_DIR="$ROOT_DIR/build-asan" ;;
  *)
    echo "Error: Unknown preset '$PRESET' (expected: debug, release, asan-ubsan)" >&2
    exit 1
    ;;
esac

if [[ "$BUILD_FIRST" -eq 1 ]]; then
  "$ROOT_DIR/scripts/build.sh" --preset "$PRESET" --jobs "$JOBS"
fi

if [[ ! -d "$BUILD_DIR" ]]; then
  echo "Error: Build directory $BUILD_DIR does not exist. Run scripts/build.sh or pass --build." >&2
  exit 1
fi

CTEST_ARGS=(--test-dir "$BUILD_DIR" --output-on-failure -j "$JOBS")
if [[ -n "$FILTER" ]]; then
  CTEST_ARGS+=(-R "$FILTER")
fi

echo "Running tests in $BUILD_DIR..."
ctest "${CTEST_ARGS[@]}"

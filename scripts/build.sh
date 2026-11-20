#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PRESET="debug"
CLEAN=0
JOBS="$(nproc 2>/dev/null || echo 4)"

usage() {
  cat <<'EOF'
Usage: scripts/build.sh [options]

Options:
  -p, --preset <name>   CMake preset to use: debug (default), release, asan-ubsan
  -j, --jobs <N>        Parallel build jobs (default: nproc)
  -c, --clean           Remove the preset build directory before configuring
  -h, --help            Show this help message
EOF
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    -p|--preset)
      PRESET="${2:?Missing value for $1}"
      shift 2
      ;;
    -j|--jobs)
      JOBS="${2:?Missing value for $1}"
      shift 2
      ;;
    -c|--clean)
      CLEAN=1
      shift
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

if [[ "$CLEAN" -eq 1 && -d "$BUILD_DIR" ]]; then
  echo "Cleaning $BUILD_DIR..."
  rm -rf "$BUILD_DIR"
fi

echo "Configuring preset '$PRESET'..."
cmake --preset "$PRESET" -S "$ROOT_DIR"

echo "Building in $BUILD_DIR (-j $JOBS)..."
cmake --build "$BUILD_DIR" -j "$JOBS"

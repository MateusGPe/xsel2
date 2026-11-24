#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${ROOT_DIR}/build-wasm"
OUT_DIR="${ROOT_DIR}/docs-site/public/wasm"

if ! command -v emcmake >/dev/null 2>&1; then
  if [[ -n "${EMSDK:-}" && -f "${EMSDK}/emsdk_env.sh" ]]; then
    # shellcheck disable=SC1091
    source "${EMSDK}/emsdk_env.sh" >/dev/null
  else
    echo "Error: emcmake not found. Source emsdk_env.sh or set EMSDK=/path/to/emsdk." >&2
    exit 1
  fi
fi

emcmake cmake -S "${ROOT_DIR}" -B "${BUILD_DIR}" \
  -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_TESTING=OFF \
  -DXSEL_BUILD_WASM=ON

cmake --build "${BUILD_DIR}" --target xsel2_wasm -j

mkdir -p "${OUT_DIR}"
cp "${BUILD_DIR}/xsel2.js" "${OUT_DIR}/xsel2.js"
cp "${BUILD_DIR}/xsel2.wasm" "${OUT_DIR}/xsel2.wasm"

echo "WASM build complete:"
ls -lh "${OUT_DIR}/xsel2.js" "${OUT_DIR}/xsel2.wasm"

#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
OUT_DIR="$ROOT_DIR/dist"
MODE="both" # both | wheel | sdist
VERIFY=0
CLEAN=0
PYTHON_VERSIONS="${PYTHON_VERSIONS:-3.13 3.14}"

usage() {
  cat <<'EOF'
Usage: scripts/pack_python.sh [options]

Build Python sdist and wheel packages for scraperpy.

Options:
  -o, --outdir <dir>     Output directory (default: dist/)
  -p, --python <vers>    Space-separated Python versions for wheels with uv (default: "3.13 3.14")
      --wheel-only       Build only the wheel(s) (.whl)
      --sdist-only       Build only the source distribution (.tar.gz)
  -t, --test             Verify built wheel(s) by running tests/test_python.py in an isolated env
  -c, --clean            Remove the output directory before building
  -h, --help             Show this help message
EOF
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    -o|--outdir)
      OUT_DIR="${2:?Missing value for $1}"
      shift 2
      ;;
    -p|--python)
      PYTHON_VERSIONS="${2:?Missing value for $1}"
      shift 2
      ;;
    --wheel-only)
      MODE="wheel"
      shift
      ;;
    --sdist-only)
      MODE="sdist"
      shift
      ;;
    -t|--test)
      VERIFY=1
      shift
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

if [[ "$CLEAN" -eq 1 && -d "$OUT_DIR" ]]; then
  echo "Cleaning $OUT_DIR..."
  rm -rf "$OUT_DIR"
fi
mkdir -p "$OUT_DIR"
OUT_DIR="$(cd "$OUT_DIR" && pwd)"

echo "==> Packing scraperpy ($MODE) into $OUT_DIR..."

if command -v uv >/dev/null 2>&1; then
  if [[ "$MODE" == "sdist" || "$MODE" == "both" ]]; then
    uv build --sdist --out-dir "$OUT_DIR" "$ROOT_DIR"
  fi
  if [[ "$MODE" == "wheel" || "$MODE" == "both" ]]; then
    for py_ver in $PYTHON_VERSIONS; do
      echo "==> Building wheel for Python $py_ver..."
      uv build --wheel --python "$py_ver" --out-dir "$OUT_DIR" "$ROOT_DIR"
    done
  fi
else
  PY_BUILD_ARGS=(--outdir "$OUT_DIR")
  case "$MODE" in
    wheel) PY_BUILD_ARGS+=(--wheel) ;;
    sdist) PY_BUILD_ARGS+=(--sdist) ;;
  esac
  python3 -m build "${PY_BUILD_ARGS[@]}" "$ROOT_DIR"
fi

echo ""
echo "==> Generating SHA256SUMS..."
(
  cd "$OUT_DIR"
  rm -f SHA256SUMS
  if command -v sha256sum >/dev/null 2>&1; then
    sha256sum *.whl *.tar.gz 2>/dev/null > SHA256SUMS || true
  elif command -v shasum >/dev/null 2>&1; then
    shasum -a 256 *.whl *.tar.gz 2>/dev/null > SHA256SUMS || true
  fi
)

if [[ "$VERIFY" -eq 1 ]]; then
  shopt -s nullglob
  WHEELS=("$OUT_DIR"/*.whl)
  shopt -u nullglob
  if [[ "${#WHEELS[@]}" -eq 0 ]]; then
    echo "Error: No wheel found in $OUT_DIR to verify." >&2
    exit 1
  fi
  for WHEEL_PATH in "${WHEELS[@]}"; do
    echo ""
    echo "==> Verifying wheel: $WHEEL_PATH"
    if command -v uv >/dev/null 2>&1; then
      UV_PY_ARGS=()
      if [[ "$(basename "$WHEEL_PATH")" =~ -cp3([0-9]+)- ]]; then
        UV_PY_ARGS=(--python "3.${BASH_REMATCH[1]}")
      fi
      (cd /tmp && uv run --isolated --no-project "${UV_PY_ARGS[@]}" --with "$WHEEL_PATH" --with pytest \
        pytest "$ROOT_DIR/tests/test_python.py" -v)
    else
      TMP_VENV="$(mktemp -d)"
      python3 -m venv "$TMP_VENV"
      "$TMP_VENV/bin/pip" install --quiet "$WHEEL_PATH" pytest
      (cd /tmp && "$TMP_VENV/bin/pytest" "$ROOT_DIR/tests/test_python.py" -v)
      rm -rf "$TMP_VENV"
    fi
  done
fi

echo ""
echo "==> Packaging complete:"
ls -lh "$OUT_DIR"

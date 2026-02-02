#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${BUILD_DIR:-$ROOT_DIR/build/coverage}"

if [[ "$(uname -s)" == "Darwin" ]]; then
  LLVM_COV="$(xcrun --find llvm-cov)"
  LLVM_PROFDATA="$(xcrun --find llvm-profdata)"
  ARCH_FLAG=(-arch "$(uname -m)")
else
  LLVM_COV="${LLVM_COV:-llvm-cov}"
  LLVM_PROFDATA="${LLVM_PROFDATA:-llvm-profdata}"
  ARCH_FLAG=()
fi

IGNORE_REGEX='(.*/tests/|.*/third_party/|.*/SDK/|.*/build/)'

rm -rf "$BUILD_DIR"
cmake -S "$ROOT_DIR" -B "$BUILD_DIR" \
  -DXP2GDL90_BUILD_TESTS=ON \
  -DXP2GDL90_ENABLE_COVERAGE=ON \
  -DCMAKE_BUILD_TYPE=Debug

cmake --build "$BUILD_DIR" --target xp2gdl90_tests --parallel

mkdir -p "$BUILD_DIR/profraw"
LLVM_PROFILE_FILE="$BUILD_DIR/profraw/%m-%p.profraw" \
  ctest --test-dir "$BUILD_DIR" --output-on-failure

shopt -s nullglob
PROFRAW_FILES=("$BUILD_DIR"/profraw/*.profraw)
shopt -u nullglob

if [[ ${#PROFRAW_FILES[@]} -eq 0 ]]; then
  echo "error: no .profraw files were produced under '$BUILD_DIR/profraw/'." >&2
  echo "This coverage script expects Clang's profiling runtime (e.g. build with clang/clang++ and -fprofile-instr-generate)." >&2
  exit 1
fi

"$LLVM_PROFDATA" merge -sparse "${PROFRAW_FILES[@]}" \
  -o "$BUILD_DIR/coverage.profdata"

SUMMARY_JSON="$("$LLVM_COV" export "$BUILD_DIR/xp2gdl90_tests" \
  "${ARCH_FLAG[@]}" \
  -instr-profile="$BUILD_DIR/coverage.profdata" \
  --summary-only \
  --ignore-filename-regex="$IGNORE_REGEX")"

XP_ROOT_DIR="$ROOT_DIR" python3 - <<'PY' <<<"$SUMMARY_JSON"
import json
import os
import sys

ROOT = os.environ["XP_ROOT_DIR"]
SRC_PREFIX = os.path.join(ROOT, "src") + os.sep

metrics = ["lines", "functions"]
totals = {m: {"count": 0, "covered": 0} for m in metrics}

data = json.load(sys.stdin)
files = data["data"][0]["files"]
for entry in files:
  filename = entry["filename"]
  if not filename.startswith(SRC_PREFIX):
    continue
  summary = entry["summary"]
  for metric in metrics:
    totals[metric]["count"] += summary[metric]["count"]
    totals[metric]["covered"] += summary[metric]["covered"]

failed = []
for metric in metrics:
  count = totals[metric]["count"]
  covered = totals[metric]["covered"]
  if count == 0:
    percent = 100.0
  else:
    percent = covered / count * 100.0
  print(f"{metric}: {percent:.2f}%")
  if percent < 100.0:
    failed.append(metric)

if failed:
  print("Coverage check failed: " + ", ".join(failed), file=sys.stderr)
  sys.exit(1)
PY

"$LLVM_COV" report "$BUILD_DIR/xp2gdl90_tests" \
  "${ARCH_FLAG[@]}" \
  -instr-profile="$BUILD_DIR/coverage.profdata" \
  --ignore-filename-regex="$IGNORE_REGEX"

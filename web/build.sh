#!/usr/bin/env bash
# =============================================================================
# web/build.sh - builds the website (used by Vercel, also works on Linux/macOS)
# -----------------------------------------------------------------------------
# 1. Installs the Emscripten C++ -> WebAssembly compiler (pinned version).
# 2. Compiles the C++ business logic from CarRentalSystem/ (the same files the
#    desktop program uses) plus web/wasm/WebBindings.cpp into rental_core.js/.wasm.
# 3. Builds the GoogleTest unit tests (tests/*.cpp) with the same compiler and
#    runs them with Node.js. If any test fails, the deployment is stopped.
# 4. Copies the HTML/CSS/JS front end and the sample data into web/dist/.
# =============================================================================
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SRC="$ROOT/CarRentalSystem"
WEB="$ROOT/web"
OUT="$WEB/dist"
EMSDK_VERSION="3.1.64"
# The SDK is installed in a temporary folder outside the project. Vercel's build
# cache can restore a *partial* copy of folders inside the project, which made
# "git clone" fail, so we always start from a clean, fresh download.
EMSDK_DIR="${EMSDK_DIR:-${TMPDIR:-/tmp}/emsdk}"

echo "==> Installing Emscripten $EMSDK_VERSION"
rm -rf "$ROOT/.emsdk"   # left over from older builds; no longer used
rm -rf "$EMSDK_DIR"
git clone --depth 1 https://github.com/emscripten-core/emsdk.git "$EMSDK_DIR"
"$EMSDK_DIR/emsdk" install "$EMSDK_VERSION"
"$EMSDK_DIR/emsdk" activate "$EMSDK_VERSION"
# shellcheck disable=SC1091
source "$EMSDK_DIR/emsdk_env.sh"

echo "==> Compiling the C++ business logic to WebAssembly"
rm -rf "$OUT"
mkdir -p "$OUT/data"

CORE_SOURCES=(
  Money.cpp
  Renter.cpp
  Rental.cpp
  Receipt.cpp
  RentalRecord.cpp
  RentalStatistics.cpp
  Vehicle.cpp
  RentalAgency.cpp
  RentalExceptions.cpp
  Validation.cpp
  TextUtils.cpp
  BusinessRuleSelfTest.cpp
  IDataStore.cpp
  TextFileDataStore.cpp
  InMemoryDataStore.cpp
  BundleDataStore.cpp
  DataFileFormat.cpp
  DateTime.cpp
  Sha256.cpp
  User.cpp
  UserDirectory.cpp
)
SOURCES=()
for file in "${CORE_SOURCES[@]}"; do SOURCES+=("$SRC/$file"); done
SOURCES+=("$WEB/wasm/WebBindings.cpp")

em++ -std=c++17 -O2 -Wall -Wextra -fexceptions \
  -I "$SRC" \
  "${SOURCES[@]}" \
  -lembind \
  -sMODULARIZE=1 \
  -sEXPORT_NAME=createRentalModule \
  -sENVIRONMENT=web \
  -sALLOW_MEMORY_GROWTH=1 \
  -sFORCE_FILESYSTEM=1 \
  -sEXPORTED_RUNTIME_METHODS=FS \
  -fexceptions \
  -o "$OUT/rental_core.js"

echo "==> Building the GoogleTest unit tests"
GTEST_VERSION="v1.14.0"
GTEST_DIR="${TMPDIR:-/tmp}/googletest"
TEST_BUILD="${TMPDIR:-/tmp}/crms-tests"
rm -rf "$GTEST_DIR" "$TEST_BUILD"
mkdir -p "$TEST_BUILD"
git clone --depth 1 --branch "$GTEST_VERSION" https://github.com/google/googletest.git "$GTEST_DIR"
GTEST_FLAGS=(-std=c++17 -O1 -fexceptions -DGTEST_HAS_PTHREAD=0
  -I "$GTEST_DIR/googletest/include" -I "$GTEST_DIR/googletest")
em++ "${GTEST_FLAGS[@]}" -c "$GTEST_DIR/googletest/src/gtest-all.cc" -o "$TEST_BUILD/gtest-all.o"
em++ "${GTEST_FLAGS[@]}" -c "$GTEST_DIR/googletest/src/gtest_main.cc" -o "$TEST_BUILD/gtest_main.o"

TEST_SOURCES=()
for file in "${CORE_SOURCES[@]}"; do TEST_SOURCES+=("$SRC/$file"); done
TEST_SOURCES+=("$ROOT"/tests/*.cpp)
em++ "${GTEST_FLAGS[@]}" -Wall -Wextra -I "$SRC"   "${TEST_SOURCES[@]}" "$TEST_BUILD/gtest-all.o" "$TEST_BUILD/gtest_main.o"   -sENVIRONMENT=node   -sALLOW_MEMORY_GROWTH=1   -sSTACK_SIZE=1MB   -sEXIT_RUNTIME=1   -o "$TEST_BUILD/unit_tests.js"

echo "==> Running the unit tests"
NODE_BIN="${EMSDK_NODE:-node}"
# 'pipefail' (set above) makes the pipeline fail when the test program fails.
if ! "$NODE_BIN" "$TEST_BUILD/unit_tests.js" --gtest_color=no | tee "$OUT/unit-tests.txt"; then
  echo "Unit tests FAILED - the website is not deployed." >&2
  exit 1
fi

echo "==> Copying the front end and sample data"
cp "$WEB/index.html" "$WEB/styles.css" "$WEB/app.js" "$OUT/"
cp "$SRC/data/vehicles.txt" "$SRC/data/revenue.txt" "$SRC/data/history.txt" "$SRC/data/vehicles_with_errors.txt" "$OUT/data/"

echo "==> Done: $(ls "$OUT")"

#!/bin/sh
# Builds the copy in ~/mangekyo that scripts/linux-suite.ps1 sends, Release and
# without the install and packaging rules, then runs the result-file test.
set -eu
cd "$(dirname "$0")/../.."
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DGPU_BENCH_ENABLE_PACKAGING=OFF
cmake --build build -j"$(nproc)"
ctest --test-dir build --output-on-failure

#!/usr/bin/env bash
# Builds and runs the Metal maths probe on a Mac. The output is printed and
# also saved under out/test-results/metal-math-probe-<time>/output.txt.
# Usage: bash scripts/metal-math-probe/run.sh [fast|precise]   (default: both)
set -euo pipefail

here="$(cd "$(dirname "$0")" && pwd)"
repo="$(cd "$here/../.." && pwd)"
[[ "$(uname -s)" == "Darwin" ]] || { echo "needs macOS with Xcode command-line tools" >&2; exit 2; }

build_dir="${TMPDIR:-/tmp}/metal-math-probe"
mkdir -p "$build_dir"
xcrun clang++ -std=c++17 -O2 -fobjc-arc -framework Foundation -framework Metal \
    "$here/metal_math_probe.mm" -o "$build_dir/metal_math_probe"

out_dir="$repo/out/test-results/metal-math-probe-$(date +%Y%m%d-%H%M%S)"
mkdir -p "$out_dir"
{
    echo "Host: $(sysctl -n hw.model) / macOS $(sw_vers -productVersion)"
    "$build_dir/metal_math_probe" "${1:-both}"
} | tee "$out_dir/output.txt"
echo "Saved: $out_dir/output.txt"

#!/usr/bin/env bash
# Build the independent CPU-only Watch app. Signing is optional for compilation.
set -euo pipefail
repo="$(cd "$(dirname "$0")/.." && pwd)"
sdk=""
while [[ $# -gt 0 ]]; do
    case "$1" in
        --sdk) sdk="${2:?--sdk requires watchos or watchsimulator}"; shift 2 ;;
        -h|--help) echo "Usage: bash scripts/build-watchos.sh [--sdk watchos|watchsimulator]"; exit 0 ;;
        *) echo "Unknown argument: $1" >&2; exit 2 ;;
    esac
done
case "$sdk" in ""|watchos|watchsimulator) ;; *) echo "Unsupported watch SDK: $sdk" >&2; exit 2 ;; esac
project="$repo/watch-gui/MangekyoWatch.xcodeproj"
if [[ ! -d "$project" ]]; then
    command -v xcodegen >/dev/null || { echo "Install XcodeGen to generate the Watch project" >&2; exit 2; }
    (cd "$repo/watch-gui" && xcodegen generate)
fi
sdks=(watchos watchsimulator)
[[ -z "$sdk" ]] || sdks=("$sdk")
for platform in "${sdks[@]}"; do
    destination="generic/platform=watchOS"
    [[ "$platform" != watchsimulator ]] || destination="generic/platform=watchOS Simulator"
    xcodebuild build -project "$project" -scheme MangekyoWatch -configuration Release \
        -sdk "$platform" -destination "$destination" \
        -derivedDataPath "$repo/build-watchos/$platform" \
        CODE_SIGNING_ALLOWED=NO CODE_SIGNING_REQUIRED=NO
done

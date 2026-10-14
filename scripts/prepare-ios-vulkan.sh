#!/usr/bin/env bash
# Fetch the pinned public-API MoltenVK XCFramework (device + simulator slices).
set -euo pipefail
REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
DEPENDENCIES="${REPO_ROOT}/build-ios/dependencies"
VERSION=1.4.2
SHA256=562a15a29bc358446a56a4091c5f7e08f604184187c1d34f712148b61ef17276
ARCHIVE="${DEPENDENCIES}/MoltenVK-all-${VERSION}.tar"
mkdir -p "$DEPENDENCIES"
if [[ ! -f "$ARCHIVE" ]]; then
    curl -fL --retry 2 "https://github.com/KhronosGroup/MoltenVK/releases/download/v${VERSION}/MoltenVK-all.tar" -o "${ARCHIVE}.partial"
    mv "${ARCHIVE}.partial" "$ARCHIVE"
fi
ACTUAL_SHA256=$(shasum -a 256 "$ARCHIVE" | cut -d ' ' -f1)
[[ "$ACTUAL_SHA256" == "$SHA256" ]] || { echo 'MoltenVK archive checksum mismatch' >&2; exit 1; }
if [[ ! -f "${DEPENDENCIES}/MoltenVK/.version-${VERSION}" ]]; then
    mkdir -p "${DEPENDENCIES}/MoltenVK"
    tar -xf "$ARCHIVE" -C "${DEPENDENCIES}/MoltenVK" --strip-components=1
    touch "${DEPENDENCIES}/MoltenVK/.version-${VERSION}"
fi
for slice in ios-arm64 ios-arm64_x86_64-simulator; do
    [[ -f "${DEPENDENCIES}/MoltenVK/MoltenVK/static/MoltenVK.xcframework/${slice}/libMoltenVK.a" ]] || exit 1
done
echo "MoltenVK ${VERSION} ready (SHA-256 verified; public-API build)."

#!/usr/bin/env bash
# build-ios.sh — Build Mangekyo for iOS (arm64).
# Usage:
#   bash scripts/build-ios.sh                        # device + simulator
#   bash scripts/build-ios.sh --sdk iphoneos         # device only
#   bash scripts/build-ios.sh --sdk iphonesimulator  # simulator only
#   bash scripts/build-ios.sh --clean                # clean first
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILD_DIR="${REPO_ROOT}/build-ios"
IOS_GUI_DIR="${REPO_ROOT}/ios-gui"
CONFIGURATION="${CONFIGURATION:-Release}"
SDK=""
CLEAN=false

while [[ $# -gt 0 ]]; do
    case "$1" in
        --sdk)           SDK="$2"; shift 2;;
        --configuration) CONFIGURATION="$2"; shift 2;;
        --clean)         CLEAN=true; shift;;
        *)               echo "Unknown option: $1"; exit 1;;
    esac
done

# Determine SDK list
if [[ -z "$SDK" ]]; then
    SDKS=("iphoneos" "iphonesimulator")
else
    SDKS=("$SDK")
fi

# Clean if requested
if $CLEAN; then
    echo "Cleaning build directory..."
    rm -rf "$BUILD_DIR"
fi

# Determine simulator arch (arm64 on Apple Silicon, x86_64 on Intel)
HOST_ARCH=$(uname -m)
SIM_ARCH="arm64"
if [[ "$HOST_ARCH" == "x86_64" ]]; then
    SIM_ARCH="x86_64"
fi

# --------------------------------------------------------------------------
# Step 1: Build libgpu_engine.a for each SDK
# --------------------------------------------------------------------------
for sdk in "${SDKS[@]}"; do
    echo ""
    echo "================================================================="
    echo "  Building libgpu_engine.a for ${sdk} (${CONFIGURATION})"
    echo "================================================================="

    if [[ "$sdk" == "iphoneos" ]]; then
        ARCHS_LIST="arm64"
        PLATFORM_NAME="iphoneos"
    else
        ARCHS_LIST="arm64;x86_64"
        PLATFORM_NAME="iphonesimulator"
    fi

    cmake_build="${BUILD_DIR}/${CONFIGURATION}-${PLATFORM_NAME}"
    mkdir -p "$cmake_build"

    # CMake configure
    cmake -S "$REPO_ROOT" -B "$cmake_build" \
        -G Xcode \
        -DCMAKE_SYSTEM_NAME=iOS \
        -DCMAKE_OSX_ARCHITECTURES="$ARCHS_LIST" \
        -DCMAKE_OSX_SYSROOT="$sdk" \
        -DCMAKE_OSX_DEPLOYMENT_TARGET=16.0 \
        -DCMAKE_BUILD_TYPE="$CONFIGURATION" \
        -DENABLE_METAL=ON

    # Build static library
    cmake --build "$cmake_build" \
        --config "$CONFIGURATION" \
        --target gpu_engine \
        -- -sdk "$sdk"

    # Find and report the built library
    LIB_PATH=$(find "$cmake_build" -name "libgpu_engine.a" -path "*/${CONFIGURATION}-${PLATFORM_NAME}/*" 2>/dev/null | head -1)
    if [[ -z "$LIB_PATH" ]]; then
        LIB_PATH=$(find "$cmake_build" -name "libgpu_engine.a" 2>/dev/null | head -1)
    fi

    if [[ -n "$LIB_PATH" ]]; then
        echo "  Static library: $LIB_PATH"
        echo "  Size: $(du -h "$LIB_PATH" | cut -f1)"
    else
        echo "  ERROR: libgpu_engine.a not found!"
        exit 1
    fi
done

# --------------------------------------------------------------------------
# Step 2: Generate Xcode project with XcodeGen
# --------------------------------------------------------------------------
echo ""
echo "================================================================="
echo "  Generating iOS Xcode project (XcodeGen)"
echo "================================================================="

if ! command -v xcodegen &>/dev/null; then
    echo "  XcodeGen not found. Installing via Homebrew..."
    brew install xcodegen
fi

cd "$IOS_GUI_DIR"
xcodegen generate
echo "  Generated: ${IOS_GUI_DIR}/Mangekyo.xcodeproj"

# --------------------------------------------------------------------------
# Step 3: Build iOS app
# --------------------------------------------------------------------------
for sdk in "${SDKS[@]}"; do
    echo ""
    echo "================================================================="
    echo "  Building Mangekyo.app for ${sdk} (${CONFIGURATION})"
    echo "================================================================="

    if [[ "$sdk" == "iphoneos" ]]; then
        ARCH="arm64"
        DESTINATION="generic/platform=iOS"
    else
        ARCH="$SIM_ARCH"
        DESTINATION="generic/platform=iOS Simulator"
    fi

    DERIVED_DATA="${BUILD_DIR}/DerivedData-${sdk}"

    xcodebuild build \
        -project "${IOS_GUI_DIR}/Mangekyo.xcodeproj" \
        -scheme Mangekyo \
        -configuration "$CONFIGURATION" \
        -destination "$DESTINATION" \
        -derivedDataPath "$DERIVED_DATA" \
        LIBRARY_SEARCH_PATHS="\"${BUILD_DIR}/${CONFIGURATION}-${sdk}\" \"${BUILD_DIR}/${CONFIGURATION}-${sdk}/${CONFIGURATION}-${sdk}\"" \
        CODE_SIGN_IDENTITY="-" \
        CODE_SIGNING_REQUIRED=NO \
        CODE_SIGNING_ALLOWED=NO \
        ONLY_ACTIVE_ARCH=YES \
        | tail -30

    # Find built app
    APP_PATH=$(find "$DERIVED_DATA" -name "Mangekyo.app" -path "*/${CONFIGURATION}-${sdk}/*" 2>/dev/null | head -1)
    if [[ -z "$APP_PATH" ]]; then
        APP_PATH=$(find "$DERIVED_DATA" -name "Mangekyo.app" 2>/dev/null | head -1)
    fi

    if [[ -n "$APP_PATH" ]]; then
        echo ""
        echo "  iOS app: $APP_PATH"
        echo "  SDK: $sdk"
        echo "  Architecture: $ARCH"
        echo "  Configuration: $CONFIGURATION"
    else
        echo "  ERROR: Mangekyo.app not found!"
        exit 1
    fi
done

echo ""
echo "================================================================="
echo "  iOS build complete."
echo "================================================================="

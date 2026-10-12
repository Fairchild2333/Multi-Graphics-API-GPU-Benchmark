#!/bin/bash
#
# Build one Metal-only macOS CLI/SwiftUI architecture slice, then stage a
# self-contained app. Apple clang can cross-compile the Intel slice on Apple
# Silicon (and vice versa); runtime smokes are deliberately limited to the
# native host slice.

set -euo pipefail

readonly SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd -P)"
readonly REPO_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd -P)"
readonly DEPLOYMENT_TARGET="12.0"
readonly HOST_ARCHITECTURE="$(uname -m)"

configuration="Release"
architecture="$(uname -m)"
build_dir=""
derived_data_dir=""
output_dir=""
regenerate_project=0

usage() {
    cat <<'EOF'
Usage: scripts/build-macos.sh [options]

Build one architecture slice of the Metal CLI and SwiftUI GUI, then create a
locally signed, relocatable Mangekyo.app containing its CLI worker and
Metal shader sources.

Options:
  --configuration NAME  Debug or Release (default: Release)
  --arch ARCH           arm64 or x86_64 (default: current host; cross-builds allowed)
  --build-dir PATH      CMake output (default: out/build/macos-<arch>-<config>)
  --derived-data PATH   Xcode DerivedData output
  --output-dir PATH     Final app directory
  --regenerate-project  Regenerate the tracked project with XcodeGen first
  -h, --help            Show this help

Environment:
  GPU_BENCH_GLFW_SOURCE_DIR  Existing GLFW 3.4 source tree (offline builds)
  MACOS_SIGN_IDENTITY        Signing identity; "-" means ad-hoc (default)

This is a single-architecture developer build. Static validation runs for both
native and cross-built slices; CLI runtime smokes run only for the host slice.
It does not notarize the app. Vulkan/MoltenVK and OpenGL are excluded.
EOF
}

absolute_from_repo() {
    case "$1" in
        /*) printf '%s\n' "$1" ;;
        *)  printf '%s/%s\n' "${REPO_ROOT}" "$1" ;;
    esac
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --configuration)
            [[ $# -ge 2 ]] || { echo "error: --configuration needs a value" >&2; exit 2; }
            configuration="$2"
            shift 2
            ;;
        --arch)
            [[ $# -ge 2 ]] || { echo "error: --arch needs a value" >&2; exit 2; }
            architecture="$2"
            shift 2
            ;;
        --build-dir)
            [[ $# -ge 2 ]] || { echo "error: --build-dir needs a value" >&2; exit 2; }
            build_dir="$(absolute_from_repo "$2")"
            shift 2
            ;;
        --derived-data)
            [[ $# -ge 2 ]] || { echo "error: --derived-data needs a value" >&2; exit 2; }
            derived_data_dir="$(absolute_from_repo "$2")"
            shift 2
            ;;
        --output-dir)
            [[ $# -ge 2 ]] || { echo "error: --output-dir needs a value" >&2; exit 2; }
            output_dir="$(absolute_from_repo "$2")"
            shift 2
            ;;
        --regenerate-project)
            regenerate_project=1
            shift
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        *)
            echo "error: unknown option: $1" >&2
            usage >&2
            exit 2
            ;;
    esac
done

case "${configuration}" in
    Debug|Release) ;;
    *)
        echo "error: configuration must be Debug or Release" >&2
        exit 2
        ;;
esac

case "${architecture}" in
    arm64|x86_64) ;;
    *)
        echo "error: architecture must be arm64 or x86_64" >&2
        exit 2
        ;;
esac

build_dir="${build_dir:-${REPO_ROOT}/out/build/macos-${architecture}-${configuration}}"
derived_data_dir="${derived_data_dir:-${REPO_ROOT}/out/xcode/macos-${architecture}-${configuration}}"
output_dir="${output_dir:-${REPO_ROOT}/out/macos/${architecture}/${configuration}}"

if [[ "$(uname -s)" != "Darwin" ]]; then
    echo "error: the macOS app must be built on macOS" >&2
    exit 1
fi

for tool in cmake xcodebuild xcrun otool install_name_tool codesign ditto lipo; do
    command -v "${tool}" >/dev/null 2>&1 || {
        echo "error: required tool not found: ${tool}" >&2
        exit 1
    }
done

project_file="${REPO_ROOT}/macos-gui/GPUBenchmark.xcodeproj"
if [[ "${regenerate_project}" -eq 1 ]]; then
    command -v xcodegen >/dev/null 2>&1 || {
        echo "error: --regenerate-project requires XcodeGen" >&2
        exit 1
    }
    (
        cd "${REPO_ROOT}/macos-gui"
        xcodegen generate --spec project.yml
    )
elif [[ ! -d "${project_file}" ]]; then
    echo "error: ${project_file} is missing; install XcodeGen and pass --regenerate-project" >&2
    exit 1
fi

echo "Configuring Metal-only engine (macOS ${DEPLOYMENT_TARGET}, ${architecture})..."
cmake_configure_args=(
    -S "${REPO_ROOT}"
    -B "${build_dir}"
    -DCMAKE_BUILD_TYPE="${configuration}"
    -DCMAKE_OSX_ARCHITECTURES="${architecture}"
    -DCMAKE_OSX_DEPLOYMENT_TARGET="${DEPLOYMENT_TARGET}"
    -DGPU_BENCH_BUILD_GLFW_FROM_SOURCE=ON
    -DENABLE_METAL=ON
    -DENABLE_VULKAN=OFF
    -DENABLE_OPENGL=OFF
    -DENABLE_DX11=OFF
    -DENABLE_DX12=OFF
    -DGPU_BENCH_ENABLE_PACKAGING=OFF
)
glfw_license="${build_dir}/_deps/glfw-src/LICENSE.md"
if [[ -n "${GPU_BENCH_GLFW_SOURCE_DIR:-}" ]]; then
    glfw_source="$(absolute_from_repo "${GPU_BENCH_GLFW_SOURCE_DIR}")"
    [[ -f "${glfw_source}/CMakeLists.txt" ]] || {
        echo "error: GPU_BENCH_GLFW_SOURCE_DIR is not a GLFW source tree: ${glfw_source}" >&2
        exit 1
    }
    glfw_license="${glfw_source}/LICENSE.md"
    cmake_configure_args+=("-DFETCHCONTENT_SOURCE_DIR_GLFW=${glfw_source}")
fi
cmake "${cmake_configure_args[@]}"
cmake --build "${build_dir}" --config "${configuration}" --parallel

cli_binary="${build_dir}/gpu_benchmark"
engine_library="${build_dir}/libgpu_engine.a"
glfw_library="${build_dir}/_deps/glfw-build/src/libglfw3.a"
[[ -x "${cli_binary}" && -f "${engine_library}" && -f "${glfw_library}" ]] || {
    echo "error: CMake did not produce the CLI, engine, and pinned GLFW archives" >&2
    exit 1
}

echo "Building SwiftUI app..."
xcodebuild \
    -project "${project_file}" \
    -scheme GPUBenchmark \
    -configuration "${configuration}" \
    -derivedDataPath "${derived_data_dir}" \
    -destination "generic/platform=macOS" \
    ARCHS="${architecture}" \
    ONLY_ACTIVE_ARCH=NO \
    CODE_SIGNING_ALLOWED=NO \
    MACOSX_DEPLOYMENT_TARGET="${DEPLOYMENT_TARGET}" \
    GPU_BENCH_NATIVE_LIB_DIR="${build_dir}" \
    GPU_BENCH_GLFW_LIB_DIR="$(dirname "${glfw_library}")" \
    build

built_app="${derived_data_dir}/Build/Products/${configuration}/Mangekyo.app"
[[ -d "${built_app}" ]] || {
    echo "error: Xcode did not produce ${built_app}" >&2
    exit 1
}

mkdir -p "${output_dir}"
stage_root="$(mktemp -d "${output_dir}/.Mangekyo-stage.XXXXXX")"
trap 'rm -rf "${stage_root:-}"' EXIT
stage_app="${stage_root}/Mangekyo.app"
ditto "${built_app}" "${stage_app}"

helpers_dir="${stage_app}/Contents/Helpers"
frameworks_dir="${stage_app}/Contents/Frameworks"
shaders_dir="${stage_app}/Contents/Resources/Shaders"
licenses_dir="${stage_app}/Contents/Resources/Licenses"
mkdir -p "${helpers_dir}" "${frameworks_dir}" "${shaders_dir}" "${licenses_dir}"
install -m 0755 "${cli_binary}" "${helpers_dir}/gpu_benchmark"
for shader in particle.metal gpu_burn.metal cinematic_liquid_v2.metal; do
    install -m 0644 "${REPO_ROOT}/shaders/${shader}" "${shaders_dir}/${shader}"
    # The engine resolves shaders beside argv[0]. Keep a bundle-internal
    # relative symlink there while the signed resource itself lives in the
    # canonical Resources directory (Helpers must contain code, not data).
    ln -s "../Resources/Shaders/${shader}" "${helpers_dir}/${shader}"
done
install -m 0644 "${REPO_ROOT}/LICENSE" "${licenses_dir}/Mangekyo-LICENSE.txt"
install -m 0644 "${REPO_ROOT}/THIRD_PARTY_NOTICES.md" \
    "${licenses_dir}/THIRD_PARTY_NOTICES.md"
install -m 0644 "${glfw_license}" \
    "${licenses_dir}/GLFW-LICENSE.md"

helper_binary="${helpers_dir}/gpu_benchmark"
if ! otool -l "${helper_binary}" | grep -Fq '@executable_path/../Frameworks'; then
    install_name_tool -add_rpath '@executable_path/../Frameworks' "${helper_binary}"
fi

verify_macho_dependencies() {
    binary="$1"
    unexpected="$(
        otool -L "${binary}" |
            awk 'NR > 1 { print $1 }' |
            grep -Ev '^(@rpath/|@loader_path/|@executable_path/|/System/Library/|/usr/lib/)' || true
    )"
    if [[ -n "${unexpected}" ]]; then
        echo "error: non-system dependency remains in ${binary}:" >&2
        echo "${unexpected}" >&2
        return 1
    fi
}

app_binary="${stage_app}/Contents/MacOS/Mangekyo"
verify_macho_dependencies "${app_binary}"
verify_macho_dependencies "${helper_binary}"

verify_architecture() {
    binary="$1"
    binary_architectures="$(lipo -archs "${binary}")"
    if [[ "${binary_architectures}" != "${architecture}" ]]; then
        echo "error: ${binary} contains '${binary_architectures}', expected only '${architecture}'" >&2
        return 1
    fi
}

for binary in \
    "${engine_library}" \
    "${glfw_library}" \
    "${app_binary}" \
    "${helper_binary}"; do
    verify_architecture "${binary}"
done

for binary in "${app_binary}" "${helper_binary}"; do
    minos="$(xcrun vtool -show-build "${binary}" | awk '/minos / {print $2; exit}')"
    if [[ "${minos}" != "${DEPLOYMENT_TARGET}" ]]; then
        echo "error: ${binary} has minimum macOS ${minos:-unknown}, expected ${DEPLOYMENT_TARGET}" >&2
        exit 1
    fi
done

source_version="$(
    sed -nE \
        's/^[[:space:]]*project\(Mangekyo[[:space:]]+VERSION[[:space:]]+([0-9]+\.[0-9]+\.[0-9]+).*/\1/p' \
        "${REPO_ROOT}/CMakeLists.txt" |
        head -n 1
)"
bundle_version="$(
    /usr/libexec/PlistBuddy \
        -c 'Print :CFBundleShortVersionString' \
        "${stage_app}/Contents/Info.plist"
)"
if [[ -z "${source_version}" || "${bundle_version}" != "${source_version}" ]]; then
    echo "error: bundle version '${bundle_version:-unknown}' does not match CMake version '${source_version:-unknown}'" >&2
    exit 1
fi

sign_identity="${MACOS_SIGN_IDENTITY:--}"
codesign --force --sign "${sign_identity}" --timestamp=none "${helper_binary}"
codesign --force --sign "${sign_identity}" --timestamp=none "${stage_app}"
codesign --verify --deep --strict --verbose=1 "${stage_app}"

if [[ "${architecture}" == "${HOST_ARCHITECTURE}" ]]; then
    smoke_dir="$(mktemp -d "${output_dir}/.Mangekyo-smoke.XXXXXX")"
    (
        cd "${smoke_dir}"
        GPU_BENCH_DATA_DIR="${smoke_dir}/data" \
            "${helper_binary}" --help >help.txt 2>&1
        GPU_BENCH_DATA_DIR="${smoke_dir}/data" \
            "${helper_binary}" --list-gpus >gpus.txt 2>&1
    )
    grep -q 'Usage:' "${smoke_dir}/help.txt" || {
        echo "error: bundled CLI smoke did not print its usage" >&2
        exit 1
    }
    grep -q '^GPU' "${smoke_dir}/gpus.txt" || {
        echo "error: bundled CLI smoke did not enumerate a Metal device" >&2
        exit 1
    }
    rm -rf "${smoke_dir}"
    runtime_verification="native CLI help + Metal enumeration passed"
else
    runtime_verification="cross-built slice; static verification only (host ${HOST_ARCHITECTURE})"
    echo "Skipping runtime smoke for ${architecture}: host architecture is ${HOST_ARCHITECTURE}."
fi

final_app="${output_dir}/Mangekyo.app"
rm -rf "${final_app}"
mv "${stage_app}" "${final_app}"
rm -rf "${stage_root}"
trap - EXIT

echo
echo "macOS build complete:"
echo "  App: ${final_app}"
echo "  CLI: ${final_app}/Contents/Helpers/gpu_benchmark"
echo "  Version: ${bundle_version}"
echo "  Architecture: ${architecture}"
echo "  API: Metal only"
echo "  Minimum OS: macOS ${DEPLOYMENT_TARGET}"
echo "  Signing: ${sign_identity} (not notarized)"
echo "  Runtime: ${runtime_verification}"

#!/bin/bash
#
# Build and validate the Apple Silicon and Intel macOS app slices, then create
# architecture-specific drag-install DMGs. Runtime smokes belong to the native
# host slice; a cross-built slice is accepted only after strict static checks.

set -euo pipefail

readonly SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd -P)"
readonly REPO_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd -P)"
readonly BUILD_SCRIPT="${SCRIPT_DIR}/build-macos.sh"
readonly DEPLOYMENT_TARGET="12.0"
readonly HOST_ARCHITECTURE="$(uname -m)"

architecture_mode="all"
output_dir=""
skip_build=0
regenerate_project=0

usage() {
    cat <<'EOF'
Usage: scripts/package-macos-dmg.sh [options]

Build and package Mangekyo 0.2.6 as separate Apple Silicon and Intel DMGs.
By default both architecture slices are rebuilt before packaging.

Options:
  --arch ARCH           all, arm64, or x86_64 (default: all)
  --output-dir PATH     DMG destination (default: out/macos/packages)
  --skip-build          Package already-validated Release app slices
  --regenerate-project  Regenerate the macOS Xcode project before the first build
  -h, --help            Show this help

Environment:
  GPU_BENCH_GLFW_SOURCE_DIR  Existing GLFW 3.4 source tree (offline builds)
  MACOS_SIGN_IDENTITY        Signing identity; "-" means ad-hoc (default)

Each DMG contains Mangekyo.app, an Applications alias, README.txt, LICENSE.txt,
and THIRD_PARTY_NOTICES.md. The script mounts every generated image and repeats
the bundle version, architecture, deployment target, dependency, and signature
checks before publishing it.
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
        --arch)
            [[ $# -ge 2 ]] || { echo "error: --arch needs a value" >&2; exit 2; }
            architecture_mode="$2"
            shift 2
            ;;
        --output-dir)
            [[ $# -ge 2 ]] || { echo "error: --output-dir needs a value" >&2; exit 2; }
            output_dir="$(absolute_from_repo "$2")"
            shift 2
            ;;
        --skip-build)
            skip_build=1
            shift
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

case "${architecture_mode}" in
    all)
        if [[ "${HOST_ARCHITECTURE}" == "x86_64" ]]; then
            architectures=(x86_64 arm64)
        else
            architectures=(arm64 x86_64)
        fi
        ;;
    arm64|x86_64)
        architectures=("${architecture_mode}")
        ;;
    *)
        echo "error: architecture must be all, arm64, or x86_64" >&2
        exit 2
        ;;
esac

if [[ "$(uname -s)" != "Darwin" ]]; then
    echo "error: DMG packages must be built on macOS" >&2
    exit 1
fi

for tool in \
    codesign ditto hdiutil install lipo otool sed shasum xcrun; do
    command -v "${tool}" >/dev/null 2>&1 || {
        echo "error: required tool not found: ${tool}" >&2
        exit 1
    }
done

[[ -x "${BUILD_SCRIPT}" ]] || {
    echo "error: build script is not executable: ${BUILD_SCRIPT}" >&2
    exit 1
}

readonly README_TEMPLATE="${REPO_ROOT}/packaging/macos/README.txt.in"
[[ -f "${README_TEMPLATE}" ]] || {
    echo "error: DMG README template is missing: ${README_TEMPLATE}" >&2
    exit 1
}

product_version="$(
    sed -nE \
        's/^[[:space:]]*project\(Mangekyo[[:space:]]+VERSION[[:space:]]+([0-9]+\.[0-9]+\.[0-9]+).*/\1/p' \
        "${REPO_ROOT}/CMakeLists.txt" |
        head -n 1
)"
[[ -n "${product_version}" ]] || {
    echo "error: could not read the Mangekyo version from CMakeLists.txt" >&2
    exit 1
}

source_plist_version="$(
    /usr/libexec/PlistBuddy \
        -c 'Print :CFBundleShortVersionString' \
        "${REPO_ROOT}/macos-gui/Info.plist"
)"
if [[ "${source_plist_version}" != "${product_version}" ]]; then
    echo "error: macOS Info.plist version '${source_plist_version}' does not match '${product_version}'" >&2
    exit 1
fi

output_dir="${output_dir:-${REPO_ROOT}/out/macos/packages}"
mkdir -p "${output_dir}"

work_root="$(mktemp -d "${TMPDIR:-/tmp}/Mangekyo-dmg.XXXXXX")"
active_mount=""
cleanup() {
    if [[ -n "${active_mount}" ]]; then
        hdiutil detach "${active_mount}" >/dev/null 2>&1 || true
    fi
    rm -rf "${work_root}"
}
trap cleanup EXIT INT TERM

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

verify_app() {
    app_path="$1"
    expected_architecture="$2"

    [[ -d "${app_path}" ]] || {
        echo "error: app is missing: ${app_path}" >&2
        return 1
    }

    plist_path="${app_path}/Contents/Info.plist"
    bundle_version="$(
        /usr/libexec/PlistBuddy \
            -c 'Print :CFBundleShortVersionString' \
            "${plist_path}"
    )"
    [[ "${bundle_version}" == "${product_version}" ]] || {
        echo "error: ${app_path} has version '${bundle_version}', expected '${product_version}'" >&2
        return 1
    }

    executable_name="$(
        /usr/libexec/PlistBuddy \
            -c 'Print :CFBundleExecutable' \
            "${plist_path}"
    )"
    app_binary="${app_path}/Contents/MacOS/${executable_name}"
    helper_binary="${app_path}/Contents/Helpers/gpu_benchmark"

    for required_path in \
        "${app_binary}" \
        "${helper_binary}" \
        "${app_path}/Contents/Resources/Shaders/particle.metal" \
        "${app_path}/Contents/Resources/Shaders/gpu_burn.metal" \
        "${app_path}/Contents/Resources/Shaders/cinematic_liquid_v2.metal" \
        "${app_path}/Contents/Resources/Licenses/Mangekyo-LICENSE.txt" \
        "${app_path}/Contents/Resources/Licenses/THIRD_PARTY_NOTICES.md" \
        "${app_path}/Contents/Resources/Licenses/GLFW-LICENSE.md"; do
        [[ -e "${required_path}" ]] || {
            echo "error: required bundle item is missing: ${required_path}" >&2
            return 1
        }
    done

    for binary in "${app_binary}" "${helper_binary}"; do
        binary_architectures="$(lipo -archs "${binary}")"
        [[ "${binary_architectures}" == "${expected_architecture}" ]] || {
            echo "error: ${binary} contains '${binary_architectures}', expected only '${expected_architecture}'" >&2
            return 1
        }

        minos="$(xcrun vtool -show-build "${binary}" | awk '/minos / {print $2; exit}')"
        [[ "${minos}" == "${DEPLOYMENT_TARGET}" ]] || {
            echo "error: ${binary} has minos '${minos:-unknown}', expected '${DEPLOYMENT_TARGET}'" >&2
            return 1
        }

        verify_macho_dependencies "${binary}"
    done

    codesign --verify --deep --strict --verbose=1 "${app_path}"
}

shared_glfw_source="${GPU_BENCH_GLFW_SOURCE_DIR:-}"
regenerate_pending="${regenerate_project}"
published_dmgs=()

for architecture in "${architectures[@]}"; do
    echo
    echo "Preparing Mangekyo ${product_version} for ${architecture}..."

    if [[ "${skip_build}" -eq 0 ]]; then
        build_args=(--configuration Release --arch "${architecture}")
        if [[ "${regenerate_pending}" -eq 1 ]]; then
            build_args+=(--regenerate-project)
            regenerate_pending=0
        fi

        if [[ -n "${shared_glfw_source}" ]]; then
            GPU_BENCH_GLFW_SOURCE_DIR="${shared_glfw_source}" \
                "${BUILD_SCRIPT}" "${build_args[@]}"
        else
            "${BUILD_SCRIPT}" "${build_args[@]}"
        fi

        fetched_source="${REPO_ROOT}/out/build/macos-${architecture}-Release/_deps/glfw-src"
        if [[ -z "${shared_glfw_source}" && -f "${fetched_source}/CMakeLists.txt" ]]; then
            shared_glfw_source="${fetched_source}"
        fi
    fi

    source_app="${REPO_ROOT}/out/macos/${architecture}/Release/Mangekyo.app"
    verify_app "${source_app}" "${architecture}"

    case "${architecture}" in
        arm64)
            architecture_label="Apple Silicon"
            ;;
        x86_64)
            architecture_label="Intel"
            ;;
    esac

    if [[ "${architecture}" == "${HOST_ARCHITECTURE}" ]]; then
        validation_text="This native slice passed CLI help and Metal device-enumeration runtime smokes on the build host."
    else
        validation_text="This slice was cross-built on a ${HOST_ARCHITECTURE} host. Architecture, minimum OS, dependencies, bundle contents, and signature were verified statically; it was not executed on ${architecture} hardware."
    fi

    stage_dir="${work_root}/stage-${architecture}"
    mkdir -p "${stage_dir}"
    ditto "${source_app}" "${stage_dir}/Mangekyo.app"
    ln -s /Applications "${stage_dir}/Applications"
    install -m 0644 "${REPO_ROOT}/LICENSE" "${stage_dir}/LICENSE.txt"
    install -m 0644 \
        "${REPO_ROOT}/THIRD_PARTY_NOTICES.md" \
        "${stage_dir}/THIRD_PARTY_NOTICES.md"
    sed \
        -e "s|@VERSION@|${product_version}|g" \
        -e "s|@ARCH_LABEL@|${architecture_label}|g" \
        -e "s|@ARCH@|${architecture}|g" \
        -e "s|@VALIDATION@|${validation_text}|g" \
        "${README_TEMPLATE}" >"${stage_dir}/README.txt"

    dmg_name="Mangekyo-${product_version}-macos-${architecture}.dmg"
    dmg_work_path="${work_root}/${dmg_name}"
    dmg_final_path="${output_dir}/${dmg_name}"
    volume_name="Mangekyo ${product_version} ${architecture}"

    hdiutil create \
        -quiet \
        -fs HFS+ \
        -format UDZO \
        -imagekey zlib-level=9 \
        -volname "${volume_name}" \
        -srcfolder "${stage_dir}" \
        "${dmg_work_path}"
    hdiutil verify "${dmg_work_path}" >/dev/null

    mount_dir="${work_root}/mount-${architecture}"
    mkdir -p "${mount_dir}"
    hdiutil attach \
        -readonly \
        -nobrowse \
        -mountpoint "${mount_dir}" \
        "${dmg_work_path}" >/dev/null
    active_mount="${mount_dir}"

    [[ -L "${mount_dir}/Applications" ]] || {
        echo "error: Applications alias is missing from ${dmg_name}" >&2
        exit 1
    }
    [[ "$(readlink "${mount_dir}/Applications")" == "/Applications" ]] || {
        echo "error: Applications alias in ${dmg_name} has the wrong target" >&2
        exit 1
    }
    for disk_item in README.txt LICENSE.txt THIRD_PARTY_NOTICES.md; do
        [[ -f "${mount_dir}/${disk_item}" ]] || {
            echo "error: ${disk_item} is missing from ${dmg_name}" >&2
            exit 1
        }
    done
    verify_app "${mount_dir}/Mangekyo.app" "${architecture}"

    hdiutil detach "${active_mount}" >/dev/null
    active_mount=""

    rm -f "${dmg_final_path}"
    mv "${dmg_work_path}" "${dmg_final_path}"
    hdiutil verify "${dmg_final_path}" >/dev/null
    published_dmgs+=("${dmg_final_path}")

    echo "Verified DMG: ${dmg_final_path}"
done

checksums_name="Mangekyo-${product_version}-macos-SHA256SUMS.txt"
checksums_work_path="${work_root}/${checksums_name}"
: >"${checksums_work_path}"
for dmg_path in "${published_dmgs[@]}"; do
    (
        cd "${output_dir}"
        shasum -a 256 "$(basename "${dmg_path}")"
    ) >>"${checksums_work_path}"
done
checksums_final_path="${output_dir}/${checksums_name}"
rm -f "${checksums_final_path}"
mv "${checksums_work_path}" "${checksums_final_path}"

echo
echo "macOS DMG packaging complete:"
for dmg_path in "${published_dmgs[@]}"; do
    echo "  ${dmg_path}"
done
echo "  Checksums: ${checksums_final_path}"
cat "${checksums_final_path}"
if [[ "${MACOS_SIGN_IDENTITY:--}" == "-" ]]; then
    echo "  Signing: ad-hoc; not notarized"
else
    echo "  Signing: ${MACOS_SIGN_IDENTITY}; notarization not performed"
fi

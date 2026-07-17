include_guard(GLOBAL)

include(GNUInstallDirs)

if(NOT TARGET gpu_benchmark)
    message(FATAL_ERROR "Packaging.cmake must be included after gpu_benchmark is defined")
endif()

# The executable still resolves shaders beside itself and some legacy report
# commands walk two directories up. Keep that contract explicit until all
# callers use PathService for read-only assets as well.
set(GPU_BENCH_INSTALL_RUNTIME_DIR "app/bin" CACHE STRING
    "Install directory for the CLI, optional GUI, DLLs and runtime shader copies")
set(GPU_BENCH_INSTALL_ASSET_DIR "assets/shaders" CACHE STRING
    "Install directory for the canonical shader asset mirror")
set(GPU_BENCH_INSTALL_SCRIPT_DIR "scripts" CACHE STRING
    "Install directory for legacy report script sources")
set(GPU_BENCH_GUI_PAYLOAD_DIR "" CACHE PATH
    "Prebuilt WinUI x64 self-contained output directory containing gpu_bench_gui.exe")
set(GPU_BENCH_RENDERDOC_DIR "" CACHE PATH
    "Optional complete RenderDoc portable directory (not a lone DLL)")
set(GPU_BENCH_REPORT_WORKER_DIR "" CACHE PATH
    "Optional frozen report-worker directory containing report_worker.exe")
set(GPU_BENCH_PACKAGE_LICENSE_FILE "${CMAKE_SOURCE_DIR}/LICENSE" CACHE FILEPATH
    "Project distribution license; required before generating a public MSI")
set(GPU_BENCH_CPACK_GENERATORS "ZIP;WIX" CACHE STRING
    "Semicolon-separated CPack generators; ZIP+WIX is the Windows release pair")

option(GPU_BENCH_BUNDLE_MSVC_RUNTIME
       "Install redistributable MSVC runtime DLLs found by CMake" ON)
option(GPU_BENCH_STRICT_RELEASE_ASSETS
       "Fail configuration when an enabled backend cannot produce required shader assets" OFF)

if(WIN32 AND NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(WARNING
        "The first Windows release skeleton is x64-only; this configuration is not x64.")
endif()

set(_gpu_bench_runtime_shaders
    ${HLSL_SOURCES}
    ${GL_SHADERS}
)
if(ENABLE_METAL AND APPLE AND METAL_FRAMEWORK)
    list(APPEND _gpu_bench_runtime_shaders
        "${CMAKE_SOURCE_DIR}/shaders/particle.metal")
endif()
list(REMOVE_DUPLICATES _gpu_bench_runtime_shaders)

set(_gpu_bench_generated_shaders ${SPV_OUTPUTS})
set(_gpu_bench_dx12_fp16_asset FALSE)
if(ENABLE_DX12 AND WIN32 AND SDK_DXC)
    set(_gpu_bench_dx12_fp16_asset TRUE)
endif()

if(GPU_BENCH_STRICT_RELEASE_ASSETS)
    if(ENABLE_VULKAN AND Vulkan_FOUND AND NOT SPV_OUTPUTS)
        message(FATAL_ERROR
            "Vulkan is enabled but no SPIR-V outputs can be produced. Install glslc "
            "on the build machine or configure with -DENABLE_VULKAN=OFF.")
    endif()
    if(ENABLE_DX12 AND WIN32 AND NOT SDK_DXC)
        message(FATAL_ERROR
            "DX12 is enabled but Windows SDK dxc.exe was not found; the FP16 "
            "SynthPeak asset would be missing from a release package.")
    endif()
endif()

install(TARGETS gpu_benchmark
    RUNTIME DESTINATION "${GPU_BENCH_INSTALL_RUNTIME_DIR}"
    COMPONENT Runtime
)

# CMake resolves DLLs represented by imported/link targets (notably the vcpkg
# dynamic GLFW build). System DLLs and the Vulkan loader are intentionally not
# copied here: vulkan-1.dll is delay-loaded on Windows, so it is optional at
# run time rather than a hard loader dependency.
if(WIN32)
    install(FILES $<TARGET_RUNTIME_DLLS:gpu_benchmark>
        DESTINATION "${GPU_BENCH_INSTALL_RUNTIME_DIR}"
        COMPONENT Runtime
        OPTIONAL
    )
endif()

if(_gpu_bench_runtime_shaders)
    # Runtime compatibility copy: current backends resolve these beside argv[0].
    install(FILES ${_gpu_bench_runtime_shaders}
        DESTINATION "${GPU_BENCH_INSTALL_RUNTIME_DIR}"
        COMPONENT Runtime
    )
    # Stable asset mirror for the future PathService-based lookup.
    install(FILES ${_gpu_bench_runtime_shaders}
        DESTINATION "${GPU_BENCH_INSTALL_ASSET_DIR}"
        COMPONENT Runtime
    )
endif()

if(_gpu_bench_generated_shaders)
    install(FILES ${_gpu_bench_generated_shaders}
        DESTINATION "${GPU_BENCH_INSTALL_RUNTIME_DIR}"
        COMPONENT Runtime
    )
    install(FILES ${_gpu_bench_generated_shaders}
        DESTINATION "${GPU_BENCH_INSTALL_ASSET_DIR}"
        COMPONENT Runtime
    )
endif()

if(_gpu_bench_dx12_fp16_asset)
    install(FILES "$<TARGET_FILE_DIR:gpu_benchmark>/synthpeak_fp16.cso"
        DESTINATION "${GPU_BENCH_INSTALL_RUNTIME_DIR}"
        COMPONENT Runtime
    )
    install(FILES "$<TARGET_FILE_DIR:gpu_benchmark>/synthpeak_fp16.cso"
        DESTINATION "${GPU_BENCH_INSTALL_ASSET_DIR}"
        COMPONENT Runtime
    )
endif()

# These are shipped as source utilities for traceability. They are not a
# deployable report feature until a frozen report_worker is supplied below.
set(_gpu_bench_report_sources
    "${CMAKE_SOURCE_DIR}/scripts/batch_benchmark.py"
    "${CMAKE_SOURCE_DIR}/scripts/compare_3dmark.py"
    "${CMAKE_SOURCE_DIR}/scripts/compare_rdoc_timing.py"
    "${CMAKE_SOURCE_DIR}/scripts/export_report.py"
    "${CMAKE_SOURCE_DIR}/scripts/extract_3dmark.py"
    "${CMAKE_SOURCE_DIR}/scripts/plot_amd_analysis.py"
    "${CMAKE_SOURCE_DIR}/scripts/plot_results.py"
    "${CMAKE_SOURCE_DIR}/scripts/plot_workloads.py"
    "${CMAKE_SOURCE_DIR}/scripts/rdoc_analyse.py"
    "${CMAKE_SOURCE_DIR}/scripts/rdoc_export_timing.py"
    "${CMAKE_SOURCE_DIR}/scripts/requirements.txt"
    "${CMAKE_SOURCE_DIR}/scripts/3dmark_scores.json"
    "${CMAKE_SOURCE_DIR}/scripts/3dmark_extracted.json"
)
install(FILES ${_gpu_bench_report_sources}
    DESTINATION "${GPU_BENCH_INSTALL_SCRIPT_DIR}"
    COMPONENT Runtime
)

set(_gpu_bench_gui_bundled FALSE)
if(GPU_BENCH_GUI_PAYLOAD_DIR)
    cmake_path(ABSOLUTE_PATH GPU_BENCH_GUI_PAYLOAD_DIR
               NORMALIZE OUTPUT_VARIABLE _gpu_bench_gui_payload)
    if(NOT EXISTS "${_gpu_bench_gui_payload}/gpu_bench_gui.exe")
        message(FATAL_ERROR
            "GPU_BENCH_GUI_PAYLOAD_DIR must contain gpu_bench_gui.exe: "
            "${_gpu_bench_gui_payload}")
    endif()
    set(_gpu_bench_gui_bundled TRUE)
    install(DIRECTORY "${_gpu_bench_gui_payload}/"
        DESTINATION "${GPU_BENCH_INSTALL_RUNTIME_DIR}"
        COMPONENT Runtime
        PATTERN "obj" EXCLUDE
        PATTERN "*.pdb" EXCLUDE
        PATTERN "*.lib" EXCLUDE
        PATTERN "*.exp" EXCLUDE
        PATTERN "*.ilk" EXCLUDE
        PATTERN "*.iobj" EXCLUDE
        PATTERN "*.ipdb" EXCLUDE
        PATTERN "*.tlog" EXCLUDE
        PATTERN "*.lastbuildstate" EXCLUDE
    )
    message(STATUS "Release GUI payload: ${_gpu_bench_gui_payload}")
else()
    message(WARNING
        "No GPU_BENCH_GUI_PAYLOAD_DIR was supplied. Install/CPack output will "
        "be CLI-only and is not the intended final GUI-first product.")
endif()

set(_gpu_bench_renderdoc_bundled FALSE)
if(GPU_BENCH_RENDERDOC_DIR)
    cmake_path(ABSOLUTE_PATH GPU_BENCH_RENDERDOC_DIR
               NORMALIZE OUTPUT_VARIABLE _gpu_bench_renderdoc_payload)
    foreach(_required_renderdoc_file renderdoccmd.exe renderdoc.dll)
        if(NOT EXISTS "${_gpu_bench_renderdoc_payload}/${_required_renderdoc_file}")
            message(FATAL_ERROR
                "RenderDoc payload is incomplete; missing ${_required_renderdoc_file} in "
                "${_gpu_bench_renderdoc_payload}")
        endif()
    endforeach()
    file(GLOB _renderdoc_license_candidates
        "${_gpu_bench_renderdoc_payload}/LICENSE*"
        "${_gpu_bench_renderdoc_payload}/license*")
    if(NOT _renderdoc_license_candidates)
        message(FATAL_ERROR
            "RenderDoc redistribution requires its license in the portable payload: "
            "${_gpu_bench_renderdoc_payload}")
    endif()
    set(_gpu_bench_renderdoc_bundled TRUE)
    install(DIRECTORY "${_gpu_bench_renderdoc_payload}/"
        DESTINATION "tools/RenderDoc"
        COMPONENT Runtime
        PATTERN "*.pdb" EXCLUDE
    )
endif()

set(_gpu_bench_report_worker_bundled FALSE)
if(GPU_BENCH_REPORT_WORKER_DIR)
    cmake_path(ABSOLUTE_PATH GPU_BENCH_REPORT_WORKER_DIR
               NORMALIZE OUTPUT_VARIABLE _gpu_bench_report_worker_payload)
    if(NOT EXISTS "${_gpu_bench_report_worker_payload}/report_worker.exe")
        message(FATAL_ERROR
            "GPU_BENCH_REPORT_WORKER_DIR must contain report_worker.exe: "
            "${_gpu_bench_report_worker_payload}")
    endif()
    set(_gpu_bench_report_worker_bundled TRUE)
    install(DIRECTORY "${_gpu_bench_report_worker_payload}/"
        DESTINATION "tools/report_worker"
        COMPONENT Runtime
        PATTERN "*.pdb" EXCLUDE
        PATTERN "__pycache__" EXCLUDE
        PATTERN "*.pyc" EXCLUDE
    )
endif()

# Bundle the compiler runtime for the ZIP/xcopy payload. This does not grant
# redistribution rights by itself; release owners must comply with the Visual
# Studio REDIST terms and use a non-preview toolchain.
set(_gpu_bench_msvc_runtime_bundled FALSE)
if(MSVC AND GPU_BENCH_BUNDLE_MSVC_RUNTIME)
    set(CMAKE_INSTALL_SYSTEM_RUNTIME_DESTINATION
        "${GPU_BENCH_INSTALL_RUNTIME_DIR}")
    set(CMAKE_INSTALL_SYSTEM_RUNTIME_COMPONENT Runtime)
    set(CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS_NO_WARNINGS TRUE)
    # CMake 4.2 may otherwise emit install(DIRECTORY "") when no auxiliary
    # runtime directory exists. Gather the list, then install only real files.
    set(CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS_SKIP TRUE)
    include(InstallRequiredSystemLibraries)
    if(CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS)
        install(PROGRAMS ${CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS}
            DESTINATION "${GPU_BENCH_INSTALL_RUNTIME_DIR}"
            COMPONENT Runtime
        )
        set(_gpu_bench_msvc_runtime_bundled TRUE)
    else()
        message(WARNING
            "CMake did not locate redistributable MSVC runtime DLLs. The staged "
            "application may require the matching VC Redist on the target machine.")
    endif()
endif()

set(_gpu_bench_project_license_bundled FALSE)
if(GPU_BENCH_PACKAGE_LICENSE_FILE)
    if(NOT EXISTS "${GPU_BENCH_PACKAGE_LICENSE_FILE}")
        message(FATAL_ERROR
            "GPU_BENCH_PACKAGE_LICENSE_FILE does not exist: "
            "${GPU_BENCH_PACKAGE_LICENSE_FILE}")
    endif()
    set(_gpu_bench_project_license_bundled TRUE)
    install(FILES "${GPU_BENCH_PACKAGE_LICENSE_FILE}"
        DESTINATION licenses
        COMPONENT Runtime
    )
endif()

function(_gpu_bench_json_bool output value)
    if(${value})
        set(${output} true PARENT_SCOPE)
    else()
        set(${output} false PARENT_SCOPE)
    endif()
endfunction()

set(_gpu_bench_vulkan_compiled FALSE)
if(ENABLE_VULKAN AND Vulkan_FOUND)
    set(_gpu_bench_vulkan_compiled TRUE)
endif()
set(_gpu_bench_dx12_compiled FALSE)
if(ENABLE_DX12 AND WIN32)
    set(_gpu_bench_dx12_compiled TRUE)
endif()
set(_gpu_bench_dx11_compiled FALSE)
if(ENABLE_DX11 AND WIN32)
    set(_gpu_bench_dx11_compiled TRUE)
endif()
set(_gpu_bench_opengl_compiled FALSE)
if(ENABLE_OPENGL AND OpenGL_FOUND)
    set(_gpu_bench_opengl_compiled TRUE)
endif()

_gpu_bench_json_bool(GPU_BENCH_MANIFEST_GUI_BUNDLED _gpu_bench_gui_bundled)
_gpu_bench_json_bool(GPU_BENCH_MANIFEST_RENDERDOC_BUNDLED _gpu_bench_renderdoc_bundled)
_gpu_bench_json_bool(GPU_BENCH_MANIFEST_REPORT_WORKER_BUNDLED _gpu_bench_report_worker_bundled)
_gpu_bench_json_bool(GPU_BENCH_MANIFEST_MSVC_RUNTIME_BUNDLED _gpu_bench_msvc_runtime_bundled)
_gpu_bench_json_bool(GPU_BENCH_MANIFEST_PROJECT_LICENSE_BUNDLED _gpu_bench_project_license_bundled)
_gpu_bench_json_bool(GPU_BENCH_MANIFEST_VULKAN_COMPILED _gpu_bench_vulkan_compiled)
_gpu_bench_json_bool(GPU_BENCH_MANIFEST_DX12_COMPILED _gpu_bench_dx12_compiled)
_gpu_bench_json_bool(GPU_BENCH_MANIFEST_DX11_COMPILED _gpu_bench_dx11_compiled)
_gpu_bench_json_bool(GPU_BENCH_MANIFEST_OPENGL_COMPILED _gpu_bench_opengl_compiled)
_gpu_bench_json_bool(GPU_BENCH_MANIFEST_DX12_FP16_ASSET _gpu_bench_dx12_fp16_asset)

if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(ARM64|arm64)$" OR CMAKE_GENERATOR_PLATFORM STREQUAL "ARM64")
        set(GPU_BENCH_MANIFEST_ARCH "arm64")
    else()
        set(GPU_BENCH_MANIFEST_ARCH "x64")
    endif()
else()
    set(GPU_BENCH_MANIFEST_ARCH "unsupported-${CMAKE_SIZEOF_VOID_P}-byte-pointer")
endif()

configure_file(
    "${CMAKE_SOURCE_DIR}/packaging/release-manifest.json.in"
    "${CMAKE_BINARY_DIR}/release-manifest.json"
    @ONLY
)
install(FILES
    "${CMAKE_BINARY_DIR}/release-manifest.json"
    "${CMAKE_SOURCE_DIR}/packaging/PACKAGE_LIMITATIONS.md"
    "${CMAKE_SOURCE_DIR}/packaging/README.md"
    DESTINATION .
    COMPONENT Runtime
)

# Keep the engineering payload redistributable for the dependencies that are
# actually copied. The project MIT license is installed separately above when
# GPU_BENCH_PACKAGE_LICENSE_FILE is set (default: repo LICENSE).
install(FILES "${CMAKE_SOURCE_DIR}/THIRD_PARTY_NOTICES.md"
    DESTINATION licenses
    COMPONENT Runtime
)
install(FILES "${CMAKE_SOURCE_DIR}/third_party/glad/LICENSE"
    DESTINATION licenses
    RENAME GLAD-LICENSE.txt
    COMPONENT Runtime
)
if(EXISTS "${glfw3_DIR}/copyright")
    install(FILES "${glfw3_DIR}/copyright"
        DESTINATION licenses
        RENAME GLFW-LICENSE.txt
        COMPONENT Runtime
    )
else()
    message(WARNING
        "The GLFW copyright file was not found next to the imported package; "
        "the release verifier will flag the missing notice.")
endif()

set(_gpu_bench_cpack_generators ${GPU_BENCH_CPACK_GENERATORS})
if(NOT GPU_BENCH_PACKAGE_LICENSE_FILE AND EXISTS "${CMAKE_SOURCE_DIR}/LICENSE")
    set(GPU_BENCH_PACKAGE_LICENSE_FILE "${CMAKE_SOURCE_DIR}/LICENSE")
endif()
if("WIX" IN_LIST _gpu_bench_cpack_generators)
    if(NOT WIN32)
        message(FATAL_ERROR "The CPack WIX generator is only available on Windows")
    endif()
    if(NOT GPU_BENCH_PACKAGE_LICENSE_FILE OR NOT EXISTS "${GPU_BENCH_PACKAGE_LICENSE_FILE}")
        message(FATAL_ERROR
            "Public MSI generation requires GPU_BENCH_PACKAGE_LICENSE_FILE to point "
            "at an existing project distribution license (expected repo LICENSE).")
    endif()
    # CPack WIX only accepts .txt/.rtf license resources; the repo uses bare LICENSE.
    get_filename_component(_gpu_bench_license_ext
        "${GPU_BENCH_PACKAGE_LICENSE_FILE}" EXT)
    if(_gpu_bench_license_ext STREQUAL ".txt" OR _gpu_bench_license_ext STREQUAL ".rtf")
        set(CPACK_RESOURCE_FILE_LICENSE "${GPU_BENCH_PACKAGE_LICENSE_FILE}")
    else()
        set(_gpu_bench_wix_license "${CMAKE_BINARY_DIR}/cpack-LICENSE.txt")
        configure_file(
            "${GPU_BENCH_PACKAGE_LICENSE_FILE}"
            "${_gpu_bench_wix_license}"
            COPYONLY)
        set(CPACK_RESOURCE_FILE_LICENSE "${_gpu_bench_wix_license}")
    endif()
    set(CPACK_WIX_UPGRADE_GUID "B8D17851-59E5-4FBA-ABF7-6A06B2CBB3DC")
    set(CPACK_WIX_PROGRAM_MENU_FOLDER "Mangekyo")
    # Prefer WiX CLI (v4+ / v5) when available; otherwise classic candle/light (v3).
    find_program(_gpu_bench_wix_cli NAMES wix)
    find_program(_gpu_bench_wix_candle NAMES candle)
    if(_gpu_bench_wix_cli)
        set(CPACK_WIX_VERSION "4")
    else()
        set(CPACK_WIX_VERSION "3")
        if(NOT _gpu_bench_wix_candle)
            message(WARNING
                "WiX tools were not found on PATH (wix.exe or candle.exe). "
                "cpack -G WIX will fail until WiX Toolset is installed.")
        endif()
    endif()
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(ARM64|arm64)$" OR CMAKE_GENERATOR_PLATFORM STREQUAL "ARM64")
        set(CPACK_WIX_ARCHITECTURE "arm64")
    else()
        set(CPACK_WIX_ARCHITECTURE "x64")
    endif()
    # Allow choosing the install directory (Program Files\\Mangekyo by default).
    set(CPACK_WIX_UI_REF "WixUI_InstallDir")
endif()

set(CPACK_GENERATOR "${GPU_BENCH_CPACK_GENERATORS}")
set(CPACK_PACKAGE_NAME "Mangekyo")
set(CPACK_PACKAGE_VENDOR "Mangekyo contributors")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY
    "Mangekyo cross-API GPU and CPU benchmark")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_INSTALL_DIRECTORY "Mangekyo")
if(WIN32)
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(ARM64|arm64)$" OR CMAKE_GENERATOR_PLATFORM STREQUAL "ARM64")
        set(CPACK_SYSTEM_NAME "windows-arm64")
    else()
        set(CPACK_SYSTEM_NAME "windows-x64")
    endif()
else()
    set(CPACK_SYSTEM_NAME "${CMAKE_SYSTEM_NAME}-${CMAKE_SYSTEM_PROCESSOR}")
endif()
set(CPACK_PACKAGE_FILE_NAME
    "${CPACK_PACKAGE_NAME}-${CPACK_PACKAGE_VERSION}-${CPACK_SYSTEM_NAME}")
set(CPACK_INCLUDE_TOPLEVEL_DIRECTORY ON)
set(CPACK_MONOLITHIC_INSTALL ON)
set(CPACK_PACKAGE_CHECKSUM SHA256)
set(CPACK_PACKAGE_DIRECTORY "${CMAKE_BINARY_DIR}/packages")

include(CPack)

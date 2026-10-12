# Mangekyo HarmonyOS native library: the NAPI bridge plus the shared gpu_engine
# (Vulkan or OpenGL ES in the window of an XComponent, and the CPU tests).
cmake_minimum_required(VERSION 3.16)
project(mangekyo_ohos CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(MANGEKYO_REPO_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../../../..")
get_filename_component(MANGEKYO_REPO_ROOT "${MANGEKYO_REPO_ROOT}" ABSOLUTE)

# A benchmark must not measure unoptimised code. The app's debug variant, the
# one that can be started over hdc, configures CMake with the Debug build
# type, which compiles the engine without optimisation: on the same CPU that
# halved the CPU scores (found 2026-10-07 on the first phone). The engine and
# the bridge get the optimisation of a release build in every variant.
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -O2 -DNDEBUG")
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -O2 -DNDEBUG")
endif()

# The root project builds gpu_engine; its OHOS branch skips GLFW and the CLI.
add_subdirectory("${MANGEKYO_REPO_ROOT}" "${CMAKE_BINARY_DIR}/mangekyo_engine" EXCLUDE_FROM_ALL)

add_library(mangekyo SHARED napi_init.cpp)
target_include_directories(mangekyo PRIVATE "${MANGEKYO_REPO_ROOT}/src")
target_link_libraries(mangekyo
    PRIVATE
        gpu_engine
        libace_napi.z.so
        libace_ndk.z.so
        libnative_window.so
        libvulkan.so
        libEGL.so
        libGLESv3.so
        libhilog_ndk.z.so
        libdeviceinfo_ndk.z.so
)

# ---- SPIR-V for the two GPU tests (stream + gpu_burn) ----
# The files go into the rawfile resources and are copied to the app's files
# directory at start-up. scripts/build-ohos.ps1 compiles them before the app is
# packaged; this target keeps them current when building from DevEco Studio,
# where a first build may package the resources before the shaders exist.
set(OHOS_SHADER_SOURCES
    "${MANGEKYO_REPO_ROOT}/shaders/particle.vert"
    "${MANGEKYO_REPO_ROOT}/shaders/particle.frag"
    "${MANGEKYO_REPO_ROOT}/shaders/compute.comp"
    "${MANGEKYO_REPO_ROOT}/shaders/fractal.vert"
    "${MANGEKYO_REPO_ROOT}/shaders/gpu_burn.frag"
)
find_program(GLSLC_EXECUTABLE glslc
    HINTS
        "$ENV{VULKAN_SDK}/Bin"
        "$ENV{VULKAN_SDK}/bin"
    NO_CMAKE_FIND_ROOT_PATH
)
set(OHOS_RAWFILE_SHADERS "${CMAKE_CURRENT_LIST_DIR}/../resources/rawfile/shaders")
file(MAKE_DIRECTORY "${OHOS_RAWFILE_SHADERS}")

# ---- OpenGL ES: the desktop OpenGL sources of the particle test and GPU Burn ----
# The OpenGL ES backend compiles them on the device and changes only their
# version line, so they travel as text. The two gpu_burn_es2 files are the GPU
# Burn that OpenGL ES 2.0 can run. scripts/build-ohos.ps1 copies them too.
foreach(GL_SHADER compute_gl.comp particle_gl.vert particle_gl.frag fractal_gl.vert gpu_burn_gl.frag
                  gpu_burn_es2.vert gpu_burn_es2.frag)
    configure_file("${MANGEKYO_REPO_ROOT}/shaders/${GL_SHADER}"
                   "${OHOS_RAWFILE_SHADERS}/${GL_SHADER}" COPYONLY)
endforeach()

if(GLSLC_EXECUTABLE)
    set(OHOS_SPV_OUTPUTS "")
    foreach(SHADER ${OHOS_SHADER_SOURCES})
        get_filename_component(SHADER_NAME "${SHADER}" NAME)
        set(SPV_FILE "${OHOS_RAWFILE_SHADERS}/${SHADER_NAME}.spv")
        add_custom_command(
            OUTPUT "${SPV_FILE}"
            COMMAND "${GLSLC_EXECUTABLE}" --target-env=vulkan1.0 "${SHADER}" -o "${SPV_FILE}"
            DEPENDS "${SHADER}"
            COMMENT "HarmonyOS SPIR-V ${SHADER_NAME}"
            VERBATIM
        )
        list(APPEND OHOS_SPV_OUTPUTS "${SPV_FILE}")
    endforeach()
    add_custom_target(mangekyo_ohos_shaders ALL DEPENDS ${OHOS_SPV_OUTPUTS})
    add_dependencies(mangekyo mangekyo_ohos_shaders)
else()
    message(WARNING "glslc not found: put prebuilt .spv files under entry/src/main/resources/rawfile/shaders/")
endif()

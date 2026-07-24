# Mangekyo Android JNI — links gpu_engine (Vulkan / ANativeWindow).
# HANDOFF: no GLFW; Vulkan via NDK stub lib; 16 KB page size; four ABIs.
cmake_minimum_required(VERSION 3.22)
project(mangekyo_android CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(MANGEKYO_REPO_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../../../..")
get_filename_component(MANGEKYO_REPO_ROOT "${MANGEKYO_REPO_ROOT}" ABSOLUTE)

# Pull in root engine as a subdirectory (ANDROID branch skips GLFW / CLI).
add_subdirectory("${MANGEKYO_REPO_ROOT}" "${CMAKE_BINARY_DIR}/mangekyo_engine" EXCLUDE_FROM_ALL)

find_library(log-lib log)
find_library(android-lib android)
# Link the NDK vulkan stub by name. Native build uses ANDROID_PLATFORM=24 so
# libvulkan exists in the sysroot; Java minSdk stays 23 with CapabilityGate.
set(VULKAN_ANDROID_LIB vulkan)

add_library(mangekyo_jni SHARED mangekyo_jni.cpp)
target_link_libraries(mangekyo_jni
    PRIVATE
        gpu_engine
        ${android-lib}
        ${log-lib}
        ${VULKAN_ANDROID_LIB}
)
target_include_directories(mangekyo_jni PRIVATE "${MANGEKYO_REPO_ROOT}/src")

# 16 KB page alignment: required for targetSdk 15+ uncompressed .so (HANDOFF).
target_link_options(mangekyo_jni PRIVATE "-Wl,-z,max-page-size=16384")

# ---- SPIR-V for vertical slice (stream + gpu_burn) ----
set(ANDROID_SHADER_SOURCES
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
        "C:/VulkanSDK/1.4.341.1/Bin"
)
set(ANDROID_ASSETS_SHADERS
    "${CMAKE_CURRENT_LIST_DIR}/../assets/shaders")
file(MAKE_DIRECTORY "${ANDROID_ASSETS_SHADERS}")

if(GLSLC_EXECUTABLE)
    set(ANDROID_SPV_OUTPUTS "")
    foreach(SHADER ${ANDROID_SHADER_SOURCES})
        get_filename_component(SHADER_NAME "${SHADER}" NAME)
        set(SPV_FILE "${ANDROID_ASSETS_SHADERS}/${SHADER_NAME}.spv")
        add_custom_command(
            OUTPUT "${SPV_FILE}"
            COMMAND "${GLSLC_EXECUTABLE}" --target-env=vulkan1.0 "${SHADER}" -o "${SPV_FILE}"
            DEPENDS "${SHADER}"
            COMMENT "Android SPIR-V ${SHADER_NAME}"
            VERBATIM
        )
        list(APPEND ANDROID_SPV_OUTPUTS "${SPV_FILE}")
    endforeach()
    add_custom_target(mangekyo_android_shaders ALL DEPENDS ${ANDROID_SPV_OUTPUTS})
    add_dependencies(mangekyo_jni mangekyo_android_shaders)
else()
    message(WARNING "glslc not found — place prebuilt .spv under app/src/main/assets/shaders/")
endif()

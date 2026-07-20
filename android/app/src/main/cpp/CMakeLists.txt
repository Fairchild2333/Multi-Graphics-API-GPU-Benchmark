# Mangekyo Android JNI stub。
# TODO(next-ai)：接入现有 gpu_engine —— add_subdirectory 指向仓库根 CMake，
# 按 HANDOFF 目标 C 第 3 条：Vulkan 用 dlopen 运行时门控（不直接链接 libvulkan），
# GL ES 3.1 走 EGL；GLFW 不参与 Android 构建。
cmake_minimum_required(VERSION 3.22)
project(mangekyo_android CXX)

add_library(mangekyo_jni SHARED mangekyo_jni.cpp)

# 16 KB 页对齐：target Android 15+ 对原生 .so 强制（HANDOFF SDK 合同）。
target_link_options(mangekyo_jni PRIVATE "-Wl,-z,max-page-size=16384")

find_library(log-lib log)
target_link_libraries(mangekyo_jni android ${log-lib})

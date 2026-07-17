cmake_minimum_required(VERSION 3.25)

# macOS: support 10.15+ (Catalina) — minimum for Metal GPUStartTime/GPUEndTime
# and MTLMathModeFast. Covers all Intel + Apple Silicon Macs from 2019 onward.
if(APPLE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "10.15" CACHE STRING "Minimum macOS version")
endif()

project(Mangekyo VERSION 0.1.3 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ---- Options ----
option(ENABLE_VULKAN "Build with Vulkan backend" ON)
if(WIN32)
    option(ENABLE_DX12  "Build with DirectX 12 backend (Windows only)" ON)
    option(ENABLE_DX11  "Build with DirectX 11 backend (Windows only)" ON)
else()
    option(ENABLE_DX12  "Build with DirectX 12 backend (Windows only)" OFF)
    option(ENABLE_DX11  "Build with DirectX 11 backend (Windows only)" OFF)
endif()
if(APPLE)
    option(ENABLE_METAL "Build with Metal backend (macOS only)" ON)
else()
    option(ENABLE_METAL "Build with Metal backend (macOS only)" OFF)
endif()
option(ENABLE_OPENGL "Build with OpenGL 4.3 backend" ON)

# ---- GLFW (required by all backends for windowing) ----
find_package(glfw3 REQUIRED)

# ---- Common source files ----
set(COMMON_SOURCES
    src/main.cpp
    src/app_base.cpp
    src/benchmark_results.cpp
    src/cpu_benchmark.cpp
    src/path_service.cpp
    src/cinematic_liquid_v2_common.cpp
)

set(BACKEND_SOURCES "")
set(BACKEND_LIBS "")
set(BACKEND_DEFS "")

# ---- Vulkan backend (optional) ----
if(ENABLE_VULKAN)
    find_package(Vulkan QUIET)
    if(Vulkan_FOUND)
        list(APPEND BACKEND_SOURCES src/vulkan_backend.cpp)
        list(APPEND BACKEND_LIBS    Vulkan::Vulkan)
        list(APPEND BACKEND_DEFS    HAVE_VULKAN GLFW_INCLUDE_VULKAN)
        message(STATUS "Vulkan backend: ENABLED")
    else()
        message(STATUS "Vulkan backend: DISABLED (Vulkan SDK not found)")
    endif()
else()
    message(STATUS "Vulkan backend: DISABLED (by option)")
endif()

# ---- DirectX 12 backend (Windows only) ----
if(ENABLE_DX12 AND WIN32)
    list(APPEND BACKEND_SOURCES src/dx12_backend.cpp)
    list(APPEND BACKEND_LIBS    d3d12 dxgi d3dcompiler)
    list(APPEND BACKEND_DEFS    HAVE_DX12)
    message(STATUS "DX12 backend:   ENABLED")
elseif(ENABLE_DX12 AND NOT WIN32)
    message(STATUS "DX12 backend:   DISABLED (not Windows)")
else()
    message(STATUS "DX12 backend:   DISABLED (by option)")
endif()

# ---- DirectX 11 backend (Windows only) ----
if(ENABLE_DX11 AND WIN32)
    list(APPEND BACKEND_SOURCES src/dx11_backend.cpp)
    list(APPEND BACKEND_LIBS    d3d11 dxgi d3dcompiler)
    list(APPEND BACKEND_DEFS    HAVE_DX11)
    message(STATUS "DX11 backend:   ENABLED")
elseif(ENABLE_DX11 AND NOT WIN32)
    message(STATUS "DX11 backend:   DISABLED (not Windows)")
else()
    message(STATUS "DX11 backend:   DISABLED (by option)")
endif()

# ---- Metal backend (macOS only) ----
if(ENABLE_METAL AND APPLE)
    enable_language(OBJCXX)
    set(CMAKE_OBJCXX_STANDARD 17)
    set(CMAKE_OBJCXX_STANDARD_REQUIRED ON)

    find_library(METAL_FRAMEWORK Metal)
    find_library(QUARTZCORE_FRAMEWORK QuartzCore)
    find_library(COCOA_FRAMEWORK Cocoa)

    if(METAL_FRAMEWORK)
        list(APPEND BACKEND_SOURCES src/metal_backend.mm src/metal_probe.mm)
        list(APPEND BACKEND_LIBS
            ${METAL_FRAMEWORK} ${QUARTZCORE_FRAMEWORK} ${COCOA_FRAMEWORK})
        list(APPEND BACKEND_DEFS HAVE_METAL)

        set_source_files_properties(src/metal_backend.mm src/metal_probe.mm
            PROPERTIES COMPILE_FLAGS "-fobjc-arc"
        )

        message(STATUS "Metal backend:  ENABLED")
    else()
        message(STATUS "Metal backend:  DISABLED (Metal framework not found)")
    endif()
elseif(ENABLE_METAL AND NOT APPLE)
    message(STATUS "Metal backend:  DISABLED (not macOS)")
else()
    message(STATUS "Metal backend:  DISABLED (by option)")
endif()

# ---- OpenGL backend (cross-platform, lowest API priority) ----
if(ENABLE_OPENGL)
    find_package(OpenGL QUIET)
    if(OpenGL_FOUND)
        # Reproducibly generated with glad v2.0.8:
        #   glad --api gl:core=4.3 --reproducible --out-path third_party/glad c
        # Keeping the generated loader in-tree removes configure-time network
        # access and the implicit Python/Jinja2 build dependency.
        add_library(glad_gl43 STATIC third_party/glad/src/gl.c)
        target_include_directories(glad_gl43 PUBLIC
            ${CMAKE_SOURCE_DIR}/third_party/glad/include)

        list(APPEND BACKEND_SOURCES src/opengl_backend.cpp)
        list(APPEND BACKEND_LIBS    OpenGL::GL glad_gl43)
        list(APPEND BACKEND_DEFS    HAVE_OPENGL)
        message(STATUS "OpenGL backend: ENABLED")
    else()
        message(STATUS "OpenGL backend: DISABLED (OpenGL not found)")
    endif()
else()
    message(STATUS "OpenGL backend: DISABLED (by option)")
endif()

# ---- Engine library (shared by the CLI and the WinUI3 C++/WinRT GUI) ----
# COMMON_SOURCES includes main.cpp, which now defines gpu_bench::cliMain (not
# main); the GUI links gpu_engine and calls cliMain in-process on a worker thread.
add_library(gpu_engine STATIC ${COMMON_SOURCES} ${BACKEND_SOURCES})
target_link_libraries(gpu_engine PUBLIC glfw ${BACKEND_LIBS})
target_compile_definitions(gpu_engine PUBLIC ${BACKEND_DEFS})
target_include_directories(gpu_engine PUBLIC ${CMAKE_SOURCE_DIR}/src)

# A Vulkan-capable build must still start on Windows machines whose driver did
# not install vulkan-1.dll.  main.cpp probes the loader before issuing any
# Vulkan call; delayimp prevents the process loader from failing before that
# capability check can select Direct3D instead.
if(MSVC AND ENABLE_VULKAN AND Vulkan_FOUND)
    target_link_libraries(gpu_engine PUBLIC delayimp)
    target_link_options(gpu_engine INTERFACE "/DELAYLOAD:vulkan-1.dll")
endif()

# ---- CLI executable (thin entry point -> gpu_bench::cliMain) ----
add_executable(gpu_benchmark src/cli_main.cpp)
target_link_libraries(gpu_benchmark PRIVATE gpu_engine)

if(MSVC)
    target_compile_options(gpu_engine PRIVATE /W4 /permissive- /utf-8)
else()
    target_compile_options(gpu_engine PRIVATE
        $<$<COMPILE_LANGUAGE:CXX>:-Wall -Wextra -Wpedantic>
        $<$<COMPILE_LANGUAGE:OBJCXX>:-Wall -Wextra>
    )
endif()

# ---- Shader compilation: GLSL -> SPIR-V (Vulkan) ----
if(ENABLE_VULKAN AND Vulkan_FOUND)
    find_program(GLSLC_EXECUTABLE glslc)
    if(GLSLC_EXECUTABLE)
        set(GLSL_SOURCES
            ${CMAKE_SOURCE_DIR}/shaders/particle.vert
            ${CMAKE_SOURCE_DIR}/shaders/particle.frag
            ${CMAKE_SOURCE_DIR}/shaders/compute.comp
            ${CMAKE_SOURCE_DIR}/shaders/nbody.comp
            ${CMAKE_SOURCE_DIR}/shaders/fractal.vert
            ${CMAKE_SOURCE_DIR}/shaders/fractal.frag
            ${CMAKE_SOURCE_DIR}/shaders/gpu_stress.frag
            ${CMAKE_SOURCE_DIR}/shaders/gpu_burn.frag
            ${CMAKE_SOURCE_DIR}/shaders/volumetric.vert
            ${CMAKE_SOURCE_DIR}/shaders/volumetric.frag
            ${CMAKE_SOURCE_DIR}/shaders/fluid_advect.comp
            ${CMAKE_SOURCE_DIR}/shaders/fluid_divergence.comp
            ${CMAKE_SOURCE_DIR}/shaders/fluid_jacobi.comp
            ${CMAKE_SOURCE_DIR}/shaders/fluid_subtract.comp
            ${CMAKE_SOURCE_DIR}/shaders/fluid_render.vert
            ${CMAKE_SOURCE_DIR}/shaders/fluid_render.frag
            ${CMAKE_SOURCE_DIR}/shaders/mls_mpm_clear_grid.comp
            ${CMAKE_SOURCE_DIR}/shaders/mls_mpm_p2g_mass_momentum.comp
            ${CMAKE_SOURCE_DIR}/shaders/mls_mpm_p2g_density_stress.comp
            ${CMAKE_SOURCE_DIR}/shaders/mls_mpm_grid_update.comp
            ${CMAKE_SOURCE_DIR}/shaders/mls_mpm_g2p.comp
            ${CMAKE_SOURCE_DIR}/shaders/cinematic_liquid_resolve.comp
            ${CMAKE_SOURCE_DIR}/shaders/cinematic_liquid_render.vert
            ${CMAKE_SOURCE_DIR}/shaders/cinematic_liquid_render.frag
            ${CMAKE_SOURCE_DIR}/shaders/mls_mpm_clear_grid_v2.comp
            ${CMAKE_SOURCE_DIR}/shaders/mls_mpm_p2g_mass_momentum_v2.comp
            ${CMAKE_SOURCE_DIR}/shaders/mls_mpm_p2g_density_stress_v2.comp
            ${CMAKE_SOURCE_DIR}/shaders/mls_mpm_grid_update_v2.comp
            ${CMAKE_SOURCE_DIR}/shaders/mls_mpm_g2p_v2.comp
            ${CMAKE_SOURCE_DIR}/shaders/cinematic_liquid_rigid_integrate_v2.comp
            ${CMAKE_SOURCE_DIR}/shaders/cinematic_liquid_resolve_v2.comp
            ${CMAKE_SOURCE_DIR}/shaders/cinematic_liquid_surface_clear_v2.comp
            ${CMAKE_SOURCE_DIR}/shaders/cinematic_liquid_surface_splat_v2.comp
            ${CMAKE_SOURCE_DIR}/shaders/cinematic_liquid_surface_resolve_v2.comp
            ${CMAKE_SOURCE_DIR}/shaders/cinematic_liquid_render_v2.vert
            ${CMAKE_SOURCE_DIR}/shaders/cinematic_liquid_render_v2.frag
            ${CMAKE_SOURCE_DIR}/shaders/cinematic_liquid_sph_external.comp
            ${CMAKE_SOURCE_DIR}/shaders/cinematic_liquid_sph_hash_count.comp
            ${CMAKE_SOURCE_DIR}/shaders/cinematic_liquid_sph_scan_block.comp
            ${CMAKE_SOURCE_DIR}/shaders/cinematic_liquid_sph_scan_add.comp
            ${CMAKE_SOURCE_DIR}/shaders/cinematic_liquid_sph_scatter.comp
            ${CMAKE_SOURCE_DIR}/shaders/cinematic_liquid_sph_density.comp
            ${CMAKE_SOURCE_DIR}/shaders/cinematic_liquid_sph_pressure.comp
            ${CMAKE_SOURCE_DIR}/shaders/cinematic_liquid_sph_viscosity.comp
            ${CMAKE_SOURCE_DIR}/shaders/cinematic_liquid_sph_integrate.comp
            ${CMAKE_SOURCE_DIR}/shaders/synthpeak_fp32.comp
            ${CMAKE_SOURCE_DIR}/shaders/synthpeak_fp64.comp
            ${CMAKE_SOURCE_DIR}/shaders/synthpeak_int32.comp
            ${CMAKE_SOURCE_DIR}/shaders/synthpeak_fp16.comp
            ${CMAKE_SOURCE_DIR}/shaders/render3d.vert
            ${CMAKE_SOURCE_DIR}/shaders/render3d.frag
        )
        set(SPV_OUTPUTS "")
        foreach(SHADER ${GLSL_SOURCES})
            get_filename_component(SHADER_NAME ${SHADER} NAME)
            set(SPV_FILE ${CMAKE_BINARY_DIR}/${SHADER_NAME}.spv)
            add_custom_command(
                OUTPUT  ${SPV_FILE}
                COMMAND ${GLSLC_EXECUTABLE} --target-env=vulkan1.1 ${SHADER} -o ${SPV_FILE}
                DEPENDS ${SHADER}
                COMMENT "Compiling ${SHADER_NAME} -> SPIR-V"
            )
            list(APPEND SPV_OUTPUTS ${SPV_FILE})
        endforeach()

        add_custom_target(compile_glsl ALL DEPENDS ${SPV_OUTPUTS})
        add_dependencies(gpu_benchmark compile_glsl)

        add_custom_command(TARGET gpu_benchmark POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different ${SPV_OUTPUTS} $<TARGET_FILE_DIR:gpu_benchmark>
            COMMENT "Copying SPIR-V files next to executable"
        )
    else()
        message(WARNING "glslc not found – Vulkan shader compilation disabled.")
    endif()
endif()

# ---- Copy HLSL shader source files next to executable (DX12/DX11) ----
if((ENABLE_DX12 AND WIN32) OR (ENABLE_DX11 AND WIN32))
    set(HLSL_SOURCES
        ${CMAKE_SOURCE_DIR}/shaders/compute.hlsl
        ${CMAKE_SOURCE_DIR}/shaders/nbody.hlsl
        ${CMAKE_SOURCE_DIR}/shaders/fractal.hlsl
        ${CMAKE_SOURCE_DIR}/shaders/gpu_stress.hlsl
        ${CMAKE_SOURCE_DIR}/shaders/gpu_burn.hlsl
        ${CMAKE_SOURCE_DIR}/shaders/volumetric.hlsl
        ${CMAKE_SOURCE_DIR}/shaders/fluid.hlsl
        ${CMAKE_SOURCE_DIR}/shaders/synthpeak_fp32.hlsl
        ${CMAKE_SOURCE_DIR}/shaders/synthpeak_fp64.hlsl
        ${CMAKE_SOURCE_DIR}/shaders/synthpeak_int32.hlsl
        ${CMAKE_SOURCE_DIR}/shaders/render3d.hlsl
        ${CMAKE_SOURCE_DIR}/shaders/particle_vs.hlsl
        ${CMAKE_SOURCE_DIR}/shaders/particle_ps.hlsl
    )
    add_custom_command(TARGET gpu_benchmark POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${HLSL_SOURCES} $<TARGET_FILE_DIR:gpu_benchmark>
        COMMENT "Copying HLSL shader files next to executable"
    )
endif()

# ---- DX12 FP16 SynthPeak: precompile to signed DXIL (.cso) via the Windows SDK
#      dxc.exe (FXC has no true float16_t; SM 6.2 + DXC is required). The SDK dxc
#      sits next to dxil.dll so the blob is signed and accepted by D3D12. ----
if(ENABLE_DX12 AND WIN32)
    file(GLOB _sdk_dxc
        "C:/Program Files (x86)/Windows Kits/10/bin/*/x64/dxc.exe"
        "C:/Program Files/Windows Kits/10/bin/*/x64/dxc.exe")
    if(_sdk_dxc)
        list(SORT _sdk_dxc)
        list(GET _sdk_dxc -1 SDK_DXC)
        message(STATUS "DX12 FP16: using DXC ${SDK_DXC}")
        add_custom_command(TARGET gpu_benchmark POST_BUILD
            COMMAND "${SDK_DXC}" -T cs_6_2 -E CSMain -enable-16bit-types
                    -Fo $<TARGET_FILE_DIR:gpu_benchmark>/synthpeak_fp16.cso
                    ${CMAKE_SOURCE_DIR}/shaders/synthpeak_fp16.hlsl
            COMMENT "Compiling synthpeak_fp16.hlsl -> signed DXIL (.cso) for DX12 FP16"
        )
    else()
        message(STATUS "DX12 FP16: Windows SDK dxc.exe not found -- FP16 SynthPeak unavailable on DX12")
    endif()
endif()

# ---- Copy Metal shader source file next to executable (macOS) ----
if(ENABLE_METAL AND APPLE AND METAL_FRAMEWORK)
    add_custom_command(TARGET gpu_benchmark POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${CMAKE_SOURCE_DIR}/shaders/particle.metal
            $<TARGET_FILE_DIR:gpu_benchmark>
        COMMENT "Copying Metal shader file next to executable"
    )
endif()

# ---- Copy OpenGL GLSL shader files next to executable ----
if(ENABLE_OPENGL AND OpenGL_FOUND)
    set(GL_SHADERS
        ${CMAKE_SOURCE_DIR}/shaders/compute_gl.comp
        ${CMAKE_SOURCE_DIR}/shaders/nbody_gl.comp
        ${CMAKE_SOURCE_DIR}/shaders/particle_gl.vert
        ${CMAKE_SOURCE_DIR}/shaders/particle_gl.frag
        ${CMAKE_SOURCE_DIR}/shaders/fractal_gl.vert
        ${CMAKE_SOURCE_DIR}/shaders/fractal_gl.frag
        ${CMAKE_SOURCE_DIR}/shaders/gpu_stress_gl.frag
        ${CMAKE_SOURCE_DIR}/shaders/gpu_burn_gl.frag
        ${CMAKE_SOURCE_DIR}/shaders/volumetric_gl.vert
        ${CMAKE_SOURCE_DIR}/shaders/volumetric_gl.frag
        ${CMAKE_SOURCE_DIR}/shaders/fluid_advect_gl.comp
        ${CMAKE_SOURCE_DIR}/shaders/fluid_divergence_gl.comp
        ${CMAKE_SOURCE_DIR}/shaders/fluid_jacobi_gl.comp
        ${CMAKE_SOURCE_DIR}/shaders/fluid_subtract_gl.comp
        ${CMAKE_SOURCE_DIR}/shaders/fluid_render_gl.vert
        ${CMAKE_SOURCE_DIR}/shaders/fluid_render_gl.frag
        ${CMAKE_SOURCE_DIR}/shaders/synthpeak_fp32_gl.comp
        ${CMAKE_SOURCE_DIR}/shaders/synthpeak_fp64_gl.comp
        ${CMAKE_SOURCE_DIR}/shaders/synthpeak_int32_gl.comp
        ${CMAKE_SOURCE_DIR}/shaders/synthpeak_fp16_gl.comp
        ${CMAKE_SOURCE_DIR}/shaders/render3d_gl.vert
        ${CMAKE_SOURCE_DIR}/shaders/render3d_gl.frag
    )
    add_custom_command(TARGET gpu_benchmark POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${GL_SHADERS} $<TARGET_FILE_DIR:gpu_benchmark>
        COMMENT "Copying OpenGL GLSL shader files next to executable"
    )
endif()

# ---- Install / release staging / CPack ----
# Packaging is kept in a separate module so normal developer builds and the
# release payload use the same target and shader lists. The first deliverable
# is the Windows x64 CLI core; GUI, RenderDoc and the frozen report worker are
# optional payloads until their remaining path/runtime blockers are removed.
option(GPU_BENCH_ENABLE_PACKAGING "Generate install and CPack release rules" ON)
if(GPU_BENCH_ENABLE_PACKAGING)
    include(cmake/Packaging.cmake)
endif()

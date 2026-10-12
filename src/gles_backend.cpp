#ifdef HAVE_GLES

#include "gles_backend.h"

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES3/gl31.h>

#if defined(__ANDROID__)
#include <android/native_window.h>
#elif defined(__OHOS__)
#include <native_window/external_window.h>
#endif

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <iterator>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#ifndef EGL_OPENGL_ES3_BIT
#define EGL_OPENGL_ES3_BIT 0x0040
#endif

namespace gpu_bench {

namespace {

// GL_EXT_disjoint_timer_query: the only way OpenGL ES tells the time on the
// GPU. Its functions are looked up by name, because a driver need not have it.
constexpr GLenum kQueryCounterBits = 0x8864;  // GL_QUERY_COUNTER_BITS_EXT
constexpr GLenum kTimestamp        = 0x8E28;  // GL_TIMESTAMP_EXT
constexpr GLenum kGpuDisjoint      = 0x8FBB;  // GL_GPU_DISJOINT_EXT

using QueryCounterFn        = void (GL_APIENTRY*)(GLuint, GLenum);
using GetQueryObjectui64vFn = void (GL_APIENTRY*)(GLuint, GLenum, GLuint64*);
using GetQueryivFn          = void (GL_APIENTRY*)(GLenum, GLenum, GLint*);

QueryCounterFn        g_queryCounter        = nullptr;
GetQueryObjectui64vFn g_getQueryObjectui64v = nullptr;

// So many voided samples in a row mean the GPU clock is of no use here.
constexpr int kMaxDisjointSamples = 64;
int g_disjointSamples = 0;

// EGL_KHR_fence_sync, for waiting on the GPU without glFinish: see WaitForGpu.
using CreateSyncFn     = EGLSyncKHR (*)(EGLDisplay, EGLenum, const EGLint*);
using DestroySyncFn    = EGLBoolean (*)(EGLDisplay, EGLSyncKHR);
using ClientWaitSyncFn = EGLint (*)(EGLDisplay, EGLSyncKHR, EGLint, EGLTimeKHR);

CreateSyncFn     g_createSync     = nullptr;
DestroySyncFn    g_destroySync    = nullptr;
ClientWaitSyncFn g_clientWaitSync = nullptr;

// A wait asks the fence without sleeping for this long, which covers a light
// frame exactly, and then at this interval, which costs a long frame nothing.
constexpr std::chrono::microseconds kWaitSpin{1000};
constexpr std::chrono::microseconds kWaitPoll{50};
constexpr std::chrono::seconds kWaitGiveUp{5};

// A shown test hands a picture to the display this often at most: see Present.
constexpr std::chrono::milliseconds kPresentInterval{33};
std::chrono::steady_clock::time_point g_lastPresent{};

[[noreturn]] void ThrowEgl(const char* what) {
    std::ostringstream message;
    message << what << " failed (EGL error 0x" << std::hex << eglGetError() << ")";
    throw std::runtime_error(message.str());
}

bool HasExtension(const char* name) {
    GLint count = 0;
    glGetIntegerv(GL_NUM_EXTENSIONS, &count);
    for (GLint i = 0; i < count; ++i) {
        const char* extension = reinterpret_cast<const char*>(
            glGetStringi(GL_EXTENSIONS, static_cast<GLuint>(i)));
        if (extension && std::strcmp(extension, name) == 0) return true;
    }
    return false;
}

std::string GlString(GLenum name) {
    const char* text = reinterpret_cast<const char*>(glGetString(name));
    return text ? text : "";
}

// The shaders are the desktop OpenGL sources. OpenGL ES wants its own version
// line and a stated precision; nothing else differs, so the work per frame is
// the same under both.
std::string ToEsSource(const std::vector<char>& bytes, const std::string& file) {
    const std::string source(bytes.begin(), bytes.end());
    const std::string desktop = "#version 430";
    if (source.compare(0, desktop.size(), desktop) != 0)
        throw std::runtime_error("Shader " + file + " does not start with '#version 430'");
    const std::size_t lineEnd = source.find('\n');
    return "#version 310 es\nprecision highp float;\nprecision highp int;\n" +
           (lineEnd == std::string::npos ? std::string() : source.substr(lineEnd + 1));
}

}  // namespace

std::uint32_t EsParticleWorkGroupSize() {
    EGLDisplay display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (display == EGL_NO_DISPLAY || !eglInitialize(display, nullptr, nullptr)) return 0;
    if (!eglBindAPI(EGL_OPENGL_ES_API)) return 0;
    const EGLint configAttributes[] = {
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,
        EGL_SURFACE_TYPE,    EGL_PBUFFER_BIT,
        EGL_NONE,
    };
    EGLConfig eglConfig = nullptr;
    EGLint configCount = 0;
    if (!eglChooseConfig(display, configAttributes, &eglConfig, 1, &configCount) || configCount < 1) return 0;
    const EGLint contextAttributes[] = { EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE };
    EGLContext context = eglCreateContext(display, eglConfig, EGL_NO_CONTEXT, contextAttributes);
    if (context == EGL_NO_CONTEXT) return 0;
    const EGLint pbufferAttributes[] = { EGL_WIDTH, 16, EGL_HEIGHT, 16, EGL_NONE };
    EGLSurface surface = eglCreatePbufferSurface(display, eglConfig, pbufferAttributes);

    std::uint32_t size = 0;
    if (surface != EGL_NO_SURFACE && eglMakeCurrent(display, surface, surface, context)) {
        GLint major = 0, minor = 0;
        glGetIntegerv(GL_MAJOR_VERSION, &major);
        glGetIntegerv(GL_MINOR_VERSION, &minor);
        if (major > 3 || (major == 3 && minor >= 1)) {
            // The same two limits, read the same way, as in CreateShaders.
            GLint maxInvocations = 0, maxSizeX = 0;
            glGetIntegerv(GL_MAX_COMPUTE_WORK_GROUP_INVOCATIONS, &maxInvocations);
            glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_SIZE, 0, &maxSizeX);
            const GLint allowed = (std::min)(maxInvocations, maxSizeX);
            if (allowed <= 0 || allowed >= static_cast<GLint>(kComputeWorkGroupSize))
                size = kComputeWorkGroupSize;
            else if (allowed >= static_cast<GLint>(kComputeWorkGroupSizeEs31))
                size = kComputeWorkGroupSizeEs31;
        }
        while (glGetError() != GL_NO_ERROR) {}
        eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    }
    if (surface != EGL_NO_SURFACE) eglDestroySurface(display, surface);
    eglDestroyContext(display, context);
    // The display belongs to the whole process and stays initialised.
    return size;
}

// -----------------------------------------------------------------------
// Shader helpers
// -----------------------------------------------------------------------

std::uint32_t GlesBackend::CompileShader(const std::string& file, std::uint32_t type) {
    return CompileSource(ToEsSource(ReadFileBytes(shaderDir_ + file), file), file, type);
}

std::uint32_t GlesBackend::CompileSource(const std::string& source, const std::string& file,
                                         std::uint32_t type) {
    GLuint shader = glCreateShader(type);
    const char* text = source.c_str();
    glShaderSource(shader, 1, &text, nullptr);
    glCompileShader(shader);

    GLint ok = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[1024]{};
        glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
        glDeleteShader(shader);
        throw std::runtime_error("Shader compilation failed (" + file + "):\n" + log);
    }
    return shader;
}

std::uint32_t GlesBackend::LinkProgram(std::uint32_t first, std::uint32_t second) {
    GLuint program = glCreateProgram();
    glAttachShader(program, first);
    if (second) glAttachShader(program, second);
    glLinkProgram(program);

    GLint ok = GL_FALSE;
    glGetProgramiv(program, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[1024]{};
        glGetProgramInfoLog(program, sizeof(log), nullptr, log);
        glDeleteProgram(program);
        throw std::runtime_error(std::string("Program link failed:\n") + log);
    }

    glDetachShader(program, first);
    glDeleteShader(first);
    if (second) { glDetachShader(program, second); glDeleteShader(second); }
    return program;
}

// -----------------------------------------------------------------------
// Init
// -----------------------------------------------------------------------

GlesBackend::~GlesBackend() {
    DestroyContext();
}

void GlesBackend::CreateContext() {
    if (!config_.headless && window_ == nullptr)
        throw std::runtime_error("OpenGL ES needs a window to draw into");

    EGLDisplay display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (display == EGL_NO_DISPLAY) ThrowEgl("eglGetDisplay");
    if (!eglInitialize(display, nullptr, nullptr)) ThrowEgl("eglInitialize");
    display_ = display;
    if (!eglBindAPI(EGL_OPENGL_ES_API)) ThrowEgl("eglBindAPI");

    const EGLint configAttributes[] = {
        EGL_RENDERABLE_TYPE, Es2() ? EGL_OPENGL_ES2_BIT : EGL_OPENGL_ES3_BIT,
        EGL_SURFACE_TYPE,    config_.headless ? EGL_PBUFFER_BIT : EGL_WINDOW_BIT,
        EGL_RED_SIZE,   8,
        EGL_GREEN_SIZE, 8,
        EGL_BLUE_SIZE,  8,
        EGL_ALPHA_SIZE, 8,
        EGL_DEPTH_SIZE,   0,
        EGL_STENCIL_SIZE, 0,
        EGL_NONE,
    };
    EGLConfig eglConfig = nullptr;
    EGLint configCount = 0;
    if (!eglChooseConfig(display, configAttributes, &eglConfig, 1, &configCount) || configCount < 1)
        ThrowEgl(Es2() ? "eglChooseConfig (OpenGL ES 2, RGBA8)" : "eglChooseConfig (OpenGL ES 3, RGBA8)");

    // Version 3 gives the newest 3.x the driver has; 3.1 is checked later.
    // Version 2 may give a newer context too: what keeps the legacy test to
    // OpenGL ES 2.0 is that its shaders and its calls are OpenGL ES 2.0.
    const EGLint contextAttributes[] = { EGL_CONTEXT_CLIENT_VERSION, Es2() ? 2 : 3, EGL_NONE };
    EGLContext context = eglCreateContext(display, eglConfig, EGL_NO_CONTEXT, contextAttributes);
    if (context == EGL_NO_CONTEXT) ThrowEgl("eglCreateContext");
    context_ = context;

    EGLSurface surface = EGL_NO_SURFACE;
    if (config_.headless) {
        const EGLint pbufferAttributes[] = { EGL_WIDTH, 16, EGL_HEIGHT, 16, EGL_NONE };
        surface = eglCreatePbufferSurface(display, eglConfig, pbufferAttributes);
        if (surface == EGL_NO_SURFACE) ThrowEgl("eglCreatePbufferSurface");
    } else {
        // A test always renders kWindowWidth x kWindowHeight, as on the desktop
        // and as the Vulkan backend does here; the system scales the picture
        // to the view.
        const int width = static_cast<int>(kWindowWidth);
        const int height = static_cast<int>(kWindowHeight);
#if defined(__ANDROID__)
        EGLint visual = 0;
        eglGetConfigAttrib(display, eglConfig, EGL_NATIVE_VISUAL_ID, &visual);
        auto* nativeWindow = static_cast<ANativeWindow*>(window_);
        ANativeWindow_setBuffersGeometry(nativeWindow, width, height, visual);
        surface = eglCreateWindowSurface(display, eglConfig, nativeWindow, nullptr);
#elif defined(__OHOS__)
        auto* nativeWindow = static_cast<OHNativeWindow*>(window_);
        OH_NativeWindow_NativeWindowHandleOpt(nativeWindow, SET_BUFFER_GEOMETRY, width, height);
        surface = eglCreateWindowSurface(
            display, eglConfig, reinterpret_cast<EGLNativeWindowType>(nativeWindow), nullptr);
#else
        (void)width;
        (void)height;
        throw std::runtime_error("OpenGL ES has no window surface on this platform");
#endif
        if (surface == EGL_NO_SURFACE) ThrowEgl("eglCreateWindowSurface");
    }
    surface_ = surface;

    if (!eglMakeCurrent(display, surface, surface, context)) ThrowEgl("eglMakeCurrent");

    if (!config_.headless) {
        EGLint width = 0, height = 0;
        eglQuerySurface(display, surface, EGL_WIDTH, &width);
        eglQuerySurface(display, surface, EGL_HEIGHT, &height);
        if (width > 0 && height > 0) {
            renderWidth_  = static_cast<std::uint32_t>(width);
            renderHeight_ = static_cast<std::uint32_t>(height);
        }

        // Swap interval 0 hands a frame over without waiting for the display.
        EGLint minInterval = 1;
        eglGetConfigAttrib(display, eglConfig, EGL_MIN_SWAP_INTERVAL, &minInterval);
        const bool intervalSet = eglSwapInterval(display, config_.vsync ? 1 : 0) == EGL_TRUE;
        const bool followsDisplay = config_.vsync || !intervalSet || minInterval > 0;
        std::cout << "[OpenGL ES] Present mode: "
                  << (followsDisplay ? "vsync (frames follow the refresh rate)"
                                     : "unsynchronised (swap interval 0, not tied to the refresh rate)")
                  << std::endl;
        if (followsDisplay && !config_.vsync) {
            std::cout << "[OpenGL ES] This device offers no unsynchronised presentation "
                         "(smallest swap interval " << minInterval
                      << (intervalSet ? "" : ", setting it failed")
                      << "); the frame rate is limited by the display." << std::endl;
        }
        if (!config_.vsync)
            std::cout << "[OpenGL ES] Every frame is drawn in full; the picture on screen is renewed about "
                      << 1000 / kPresentInterval.count() << " times a second." << std::endl;
        presentFollowsDisplay_ = followsDisplay;
    }
}

void GlesBackend::DestroyContext() {
    if (!display_) return;
    EGLDisplay display = static_cast<EGLDisplay>(display_);
    eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    if (surface_) eglDestroySurface(display, static_cast<EGLSurface>(surface_));
    if (context_) eglDestroyContext(display, static_cast<EGLContext>(context_));
    // The display itself is left alone: the app around the engine may be
    // drawing its own interface through the same one.
    eglReleaseThread();
    surface_ = nullptr;
    context_ = nullptr;
    display_ = nullptr;
}

void GlesBackend::InitBackend() {
    if (Es2() && !isGpuBurnWorkload(config_.workload))
        throw std::runtime_error("OpenGL ES 2.0 runs GPU Burn only");
    if (config_.workload != Workload::Stream && !isGpuBurnWorkload(config_.workload))
        throw std::runtime_error("OpenGL ES runs the particle test and GPU Burn only");

    std::cout << "[OpenGL ES Init] Creating the context..." << std::endl;
    CreateContext();

    deviceName_ = GlString(GL_RENDERER);
    driverVersion_ = GlString(GL_VERSION);
    std::cout << "[OpenGL ES Init] " << deviceName_ << "  |  " << driverVersion_ << std::endl;
    FindGpuWait();
    g_lastPresent = {};

    if (Es2()) {
        std::cout << "[OpenGL ES Init] Compiling the OpenGL ES 2.0 GPU Burn ("
                  << config_.gpuBurnIter << " steps)..." << std::endl;
        CreateEs2Burn();
        // The legacy tier has one way of timing on every device, old or new.
        synchronizedTimingFallback_ = true;
        std::cout << "[Profiling] OpenGL ES 2.0: frames are timed by wall clock." << std::endl;
        // The two draws of a frame add up; see gpu_burn_es2.frag.
        glEnable(GL_BLEND);
        glBlendFunc(GL_ONE, GL_ONE);
        glViewport(0, 0, static_cast<GLsizei>(renderWidth_), static_cast<GLsizei>(renderHeight_));
        std::cout << "[OpenGL ES Init] Initialisation complete." << std::endl;
        return;
    }

    GLint major = 0, minor = 0;
    glGetIntegerv(GL_MAJOR_VERSION, &major);
    glGetIntegerv(GL_MINOR_VERSION, &minor);
    if (major < 3 || (major == 3 && minor < 1)) {
        throw std::runtime_error("OpenGL ES 3.1 is required for compute shaders; this device offers " +
                                 std::to_string(major) + "." + std::to_string(minor));
    }

    std::cout << "[OpenGL ES Init] Compiling shaders..." << std::endl;
    CreateShaders();
    std::cout << "[OpenGL ES Init] Creating particle buffers..." << std::endl;
    CreateParticleBuffers();
    std::cout << "[OpenGL ES Init] Creating timestamp queries..." << std::endl;
    CreateTimestampQueries();

    if (!config_.headless) {
        if (isGpuBurnWorkload(config_.workload)) {
            glDisable(GL_BLEND);
        } else {
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        }
        glViewport(0, 0, static_cast<GLsizei>(renderWidth_), static_cast<GLsizei>(renderHeight_));
    }

    CheckTimestampsAgainstWallClock();
    CheckWaitCoversCompute();

    std::cout << "[OpenGL ES Init] Initialisation complete." << std::endl;
}

void GlesBackend::CreateShaders() {
    if (config_.workload == Workload::Stream) {
        // The shader's work groups have kComputeWorkGroupSize invocations.
        // OpenGL ES 3.1 promises 128 only, and a Mali-T860 (2026-10-08) allows
        // no more: its compiler turns the shader down. On such a driver the
        // test runs in groups of 128 instead, twice as many of them, for the
        // same particles and the same work on each. That is not the same
        // shader: on an Adreno 740, which runs both, groups of 128 measured
        // the same at 65,536 particles and 2-3% faster at a million and more.
        // So the result is saved in a group of its own and says so.
        GLint maxInvocations = 0, maxSizeX = 0;
        glGetIntegerv(GL_MAX_COMPUTE_WORK_GROUP_INVOCATIONS, &maxInvocations);
        glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_SIZE, 0, &maxSizeX);
        while (glGetError() != GL_NO_ERROR) {}
        const GLint allowed = (std::min)(maxInvocations, maxSizeX);
        if (allowed > 0 && allowed < static_cast<GLint>(config_.computeWorkGroupSize)) {
            if (allowed < static_cast<GLint>(kComputeWorkGroupSizeEs31)) {
                throw std::runtime_error(
                    "the particle test needs compute work groups of at least " +
                    std::to_string(kComputeWorkGroupSizeEs31) + " invocations; this OpenGL ES driver allows " +
                    std::to_string(allowed));
            }
            std::cout << "[OpenGL ES Init] This driver allows compute work groups of " << allowed
                      << " invocations, fewer than the " << config_.computeWorkGroupSize
                      << " of the particle test." << std::endl;
            config_.computeWorkGroupSize = kComputeWorkGroupSizeEs31;
        }
        std::string source = ToEsSource(ReadFileBytes(shaderDir_ + "compute_gl.comp"), "compute_gl.comp");
        if (config_.computeWorkGroupSize != kComputeWorkGroupSize) {
            const std::string written = "local_size_x = " + std::to_string(kComputeWorkGroupSize);
            const std::size_t at = source.find(written);
            if (at == std::string::npos)
                throw std::runtime_error("compute_gl.comp does not say '" + written + "'");
            source.replace(at, written.size(), "local_size_x = " + std::to_string(config_.computeWorkGroupSize));
            std::cout << "[OpenGL ES Init] The particle test runs in work groups of " << config_.computeWorkGroupSize
                      << " invocations instead of " << kComputeWorkGroupSize
                      << ". Its result goes to a score group of its own (_wg" << config_.computeWorkGroupSize
                      << ") and is not one of the results in groups of " << kComputeWorkGroupSize << "."
                      << std::endl;
        }
        const auto compute = CompileSource(source, "compute_gl.comp", GL_COMPUTE_SHADER);
        computeProgram_ = LinkProgram(compute, 0);
    }
    if (!config_.headless) {
        const bool burn = isGpuBurnWorkload(config_.workload);
        const auto vertex   = CompileShader(burn ? "fractal_gl.vert"  : "particle_gl.vert", GL_VERTEX_SHADER);
        const auto fragment = CompileShader(burn ? "gpu_burn_gl.frag" : "particle_gl.frag", GL_FRAGMENT_SHADER);
        renderProgram_ = LinkProgram(vertex, fragment);
    }
}

void GlesBackend::CreateParticleBuffers() {
    if (config_.workload == Workload::Stream) {
        const GLsizeiptr bufferSize =
            static_cast<GLsizeiptr>(sizeof(Particle) * config_.particleCount);

        // The test dispatches all its work groups at once and keeps all its
        // particles in one block; a driver may allow less than a size needs.
        const std::uint32_t groups =
            (config_.particleCount + config_.computeWorkGroupSize - 1) / config_.computeWorkGroupSize;
        GLint maxGroups = 0;
        glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_COUNT, 0, &maxGroups);
        if (maxGroups > 0 && groups > static_cast<std::uint32_t>(maxGroups)) {
            throw std::runtime_error(
                std::to_string(config_.particleCount) + " particles need " + std::to_string(groups) +
                " compute work groups in one dispatch; this OpenGL ES driver allows " +
                std::to_string(maxGroups));
        }
        GLint64 maxBlock = 0;
        glGetInteger64v(GL_MAX_SHADER_STORAGE_BLOCK_SIZE, &maxBlock);
        if (maxBlock > 0 && static_cast<GLint64>(bufferSize) > maxBlock) {
            throw std::runtime_error(
                std::to_string(config_.particleCount) + " particles need a storage block of " +
                std::to_string(bufferSize / (1024 * 1024)) + " MiB; this OpenGL ES driver allows " +
                std::to_string(maxBlock / (1024 * 1024)) + " MiB");
        }

        while (glGetError() != GL_NO_ERROR) {}
        glGenBuffers(1, &ssbo_);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo_);
        glBufferData(GL_SHADER_STORAGE_BUFFER, bufferSize, initialParticles_.data(), GL_DYNAMIC_COPY);
        if (glGetError() != GL_NO_ERROR) {
            throw std::runtime_error("The driver could not allocate the particle buffer (" +
                                     std::to_string(bufferSize / (1024 * 1024)) + " MiB)");
        }
        std::cout << "Created particle buffers: " << config_.particleCount
                  << " particles" << std::endl;
    }

    if (!config_.headless) {
        glGenVertexArrays(1, &vao_);
        if (config_.workload == Workload::Stream) {
            // The storage buffer is also the vertex buffer of the picture.
            glBindVertexArray(vao_);
            glBindBuffer(GL_ARRAY_BUFFER, ssbo_);
            // location 0 = position (vec4), location 1 = velocity (vec4)
            glEnableVertexAttribArray(0);
            glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE,
                                  sizeof(Particle), reinterpret_cast<void*>(0));
            glEnableVertexAttribArray(1);
            glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE,
                                  sizeof(Particle),
                                  reinterpret_cast<void*>(offsetof(Particle, vx)));
            glBindVertexArray(0);
        }
        // GPU Burn draws one triangle from gl_VertexID: its array stays empty.
    }

    // 16 bytes of parameters, std140: the particle step or the GPU Burn pass.
    glGenBuffers(1, &ubo_);
    glBindBuffer(GL_UNIFORM_BUFFER, ubo_);
    const float params[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    glBufferData(GL_UNIFORM_BUFFER, sizeof(params), params, GL_DYNAMIC_DRAW);
}

void GlesBackend::CreateEs2Burn() {
    // GLSL ES 1.00 wants a constant loop bound, so the step count is compiled in.
    const std::vector<char> vertexBytes = ReadFileBytes(shaderDir_ + "gpu_burn_es2.vert");
    const std::vector<char> fragmentBytes = ReadFileBytes(shaderDir_ + "gpu_burn_es2.frag");
    const std::string vertexSource(vertexBytes.begin(), vertexBytes.end());
    const std::string fragmentSource =
        "#define BURN_STEPS " + std::to_string(config_.gpuBurnIter) + "\n" +
        std::string(fragmentBytes.begin(), fragmentBytes.end());

    const auto vertex = CompileSource(vertexSource, "gpu_burn_es2.vert", GL_VERTEX_SHADER);
    const auto fragment = CompileSource(fragmentSource, "gpu_burn_es2.frag", GL_FRAGMENT_SHADER);
    renderProgram_ = LinkProgram(vertex, fragment);

    positionSlot_ = glGetAttribLocation(renderProgram_, "aPosition");
    phaseUniform_ = glGetUniformLocation(renderProgram_, "uPhase");
    passUniform_  = glGetUniformLocation(renderProgram_, "uPass");
    if (positionSlot_ < 0)
        throw std::runtime_error("gpu_burn_es2.vert has no attribute aPosition");

    // One triangle that covers the screen.
    const float corners[6] = { -1.0f, -1.0f,  3.0f, -1.0f,  -1.0f, 3.0f };
    glGenBuffers(1, &triangleVbo_);
    glBindBuffer(GL_ARRAY_BUFFER, triangleVbo_);
    glBufferData(GL_ARRAY_BUFFER, sizeof(corners), corners, GL_STATIC_DRAW);
}

void GlesBackend::DrawEs2BurnFrame(float deltaTime) {
    using Clock = std::chrono::steady_clock;
    const auto frameBegin = Clock::now();

    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    glUseProgram(renderProgram_);
    glBindBuffer(GL_ARRAY_BUFFER, triangleVbo_);
    glEnableVertexAttribArray(static_cast<GLuint>(positionSlot_));
    glVertexAttribPointer(static_cast<GLuint>(positionSlot_), 2, GL_FLOAT, GL_FALSE, 0, nullptr);

    // The shader works in mediump: the phase is kept inside one turn here.
    burnElapsed_ += deltaTime;
    glUniform1f(phaseUniform_, std::fmod(burnElapsed_ * 0.7f, 6.2831853f));
    for (std::uint32_t pass = 0; pass < gpuBurnDrawsPerFrame(config_.workload); ++pass) {
        glUniform1f(passUniform_, static_cast<float>(pass));
        glDrawArrays(GL_TRIANGLES, 0, 3);
    }

    WaitForGpu();
    const double renderMs =
        std::chrono::duration<double, std::milli>(Clock::now() - frameBegin).count();
    AccumulateTiming(0.0, renderMs, renderMs);

    Present();
    ++currentFrame_;
}

void GlesBackend::CreateTimestampQueries() {
    g_queryCounter = nullptr;
    g_getQueryObjectui64v = nullptr;
    g_disjointSamples = 0;

    if (!HasExtension("GL_EXT_disjoint_timer_query")) {
        EnableSynchronizedTimingFallback("GL_EXT_disjoint_timer_query is not offered");
        return;
    }
    g_queryCounter = reinterpret_cast<QueryCounterFn>(eglGetProcAddress("glQueryCounterEXT"));
    g_getQueryObjectui64v =
        reinterpret_cast<GetQueryObjectui64vFn>(eglGetProcAddress("glGetQueryObjectui64vEXT"));
    const auto getQueryiv = reinterpret_cast<GetQueryivFn>(eglGetProcAddress("glGetQueryivEXT"));
    if (!g_queryCounter || !g_getQueryObjectui64v || !getQueryiv) {
        EnableSynchronizedTimingFallback("the timer query functions are missing");
        return;
    }

    // Some drivers offer the extension for elapsed time only.
    GLint bits = 0;
    getQueryiv(kTimestamp, kQueryCounterBits, &bits);
    while (glGetError() != GL_NO_ERROR) {}
    timestampsSupported_ = bits > 0;
    if (!timestampsSupported_) {
        EnableSynchronizedTimingFallback("timestamp queries are not supported");
        return;
    }

    for (int slot = 0; slot < kTimestampSlotCount; ++slot)
        glGenQueries(kTimestampsPerFrame, timestampQueries_[slot]);
    timestampQueriesAllocated_ = true;

    // Reading the flag clears what was left over from before this run.
    GLint disjoint = 0;
    glGetIntegerv(kGpuDisjoint, &disjoint);

    std::cout << "[Profiling] OpenGL ES timestamp queries enabled (" << bits << " bits)." << std::endl;
}

// A timestamp is meant to be taken once the GPU has done everything before
// it. Some drivers take it when the command is handed over instead: on an
// Adreno 740 (Quest 3, 2026-10-07) the two stamps around a compute dispatch
// were a microsecond apart and the work showed up later in the frame, which
// made 65,536 particles read 2,500 GB/s. So before a run, the work of the test
// is timed both ways at once, and the queries are used only if the GPU's own
// figure covers at least half of what the wall clock saw.
void GlesBackend::CheckTimestampsAgainstWallClock() {
    if (!UseTimestampQueries()) return;
    using Clock = std::chrono::steady_clock;

    const bool burn = isGpuBurnWorkload(config_.workload);
    // Enough work that waiting for the GPU does not dominate the wall clock:
    // about four million particle steps, or one frame of GPU Burn.
    const std::uint32_t repeats = burn ? 1u
        : (std::min)(64u, (std::max)(1u, (4194304u + config_.particleCount - 1) / config_.particleCount));
    const GLuint groups =
        (config_.particleCount + config_.computeWorkGroupSize - 1) / config_.computeWorkGroupSize;

    GLuint queries[2]{};
    glGenQueries(2, queries);

    double bestRatio = 0.0, bestGpuMs = 0.0, bestWallMs = 0.0;
    for (int attempt = 0; attempt < 3; ++attempt) {
        WaitForGpu();
        const auto begin = Clock::now();
        // The stamps sit where a frame of the test puts them. For GPU Burn
        // that is after the clear, inside the pass: a GPU that renders in
        // tiles puts the whole pass off, and both stamps then say the same.
        if (burn) {
            glClearColor(0.0f, 0.0f, 0.02f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT);
        }
        g_queryCounter(queries[0], kTimestamp);
        if (burn) {
            glUseProgram(renderProgram_);
            glBindBufferBase(GL_UNIFORM_BUFFER, 1, ubo_);
            glBindVertexArray(vao_);
            glBindBuffer(GL_UNIFORM_BUFFER, ubo_);
            for (std::uint32_t pass = 0; pass < gpuBurnDrawsPerFrame(config_.workload); ++pass) {
                const GpuBurnParams params{ 0.0f, static_cast<float>(pass),
                                            config_.gpuBurnIter, gpuBurnShaderVersion(config_.workload) };
                glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(params), &params);
                glDrawArrays(GL_TRIANGLES, 0, 3);
            }
        } else {
            // A step of zero length leaves every particle where it is.
            const float params[4] = {0.0f, 1.0f, 0.0f, 0.0f};
            glBindBuffer(GL_UNIFORM_BUFFER, ubo_);
            glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(params), params);
            glUseProgram(computeProgram_);
            glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, ssbo_);
            glBindBufferBase(GL_UNIFORM_BUFFER, 1, ubo_);
            for (std::uint32_t i = 0; i < repeats; ++i) {
                glDispatchCompute(groups, 1, 1);
                glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
            }
        }
        g_queryCounter(queries[1], kTimestamp);
        WaitForGpu();
        const double wallMs = std::chrono::duration<double, std::milli>(Clock::now() - begin).count();

        GLuint64 stamps[2]{};
        g_getQueryObjectui64v(queries[0], GL_QUERY_RESULT, &stamps[0]);
        g_getQueryObjectui64v(queries[1], GL_QUERY_RESULT, &stamps[1]);
        GLint disjoint = 0;
        glGetIntegerv(kGpuDisjoint, &disjoint);
        if (disjoint || stamps[1] < stamps[0] || wallMs <= 0.0) continue;

        const double gpuMs = static_cast<double>(stamps[1] - stamps[0]) / 1e6;
        if (gpuMs / wallMs >= bestRatio) {
            bestRatio = gpuMs / wallMs;
            bestGpuMs = gpuMs;
            bestWallMs = wallMs;
        }
    }
    glDeleteQueries(2, queries);
    while (glGetError() != GL_NO_ERROR) {}

    if (bestWallMs <= 0.0) {
        // Every attempt was voided or came back out of order, as on a
        // Mali-T860 (2026-10-08); there are no two figures to print.
        EnableSynchronizedTimingFallback("no usable reading in three attempts");
        return;
    }
    std::cout << "[Profiling] Timestamp check: " << bestGpuMs << " ms by the GPU's clock, "
              << bestWallMs << " ms by wall clock." << std::endl;
    if (bestRatio < 0.5)
        EnableSynchronizedTimingFallback("its timestamps do not cover the GPU's work");
}

bool GlesBackend::UseTimestampQueries() const {
    return timestampsSupported_ && !synchronizedTimingFallback_;
}

void GlesBackend::EnableSynchronizedTimingFallback(const char* reason) {
    if (synchronizedTimingFallback_) return;
    synchronizedTimingFallback_ = true;
    timestampFrameCount_ = 0;
    std::cout << "[Profiling] OpenGL ES timestamp queries are unreliable ("
              << reason << "); using synchronized wall-clock timing."
              << std::endl;
}

void GlesBackend::FindGpuWait() {
    g_createSync = nullptr;
    g_destroySync = nullptr;
    g_clientWaitSync = nullptr;
    const char* extensions = eglQueryString(static_cast<EGLDisplay>(display_), EGL_EXTENSIONS);
    if (extensions && std::strstr(extensions, "EGL_KHR_fence_sync")) {
        g_createSync = reinterpret_cast<CreateSyncFn>(eglGetProcAddress("eglCreateSyncKHR"));
        g_destroySync = reinterpret_cast<DestroySyncFn>(eglGetProcAddress("eglDestroySyncKHR"));
        g_clientWaitSync = reinterpret_cast<ClientWaitSyncFn>(eglGetProcAddress("eglClientWaitSyncKHR"));
    }
    if (!g_createSync || !g_destroySync || !g_clientWaitSync) {
        g_createSync = nullptr;
        std::cout << "[Profiling] No EGL fence on this device: waiting for the GPU with glFinish, "
                     "which may round a wait up to the system's timer tick." << std::endl;
    }
}

// Waits until the GPU has done everything issued so far; wall-clock timing
// ends its measurement with it.
//
// glFinish is not used for that: a driver may sleep in whole timer ticks inside
// it. On a Snapdragon 8 Gen 2 phone (2026-10-07) every glFinish took a multiple
// of 4 ms, the tick of its kernel, so a 0.2 ms particle step read 4.3 ms and
// the score was a twentieth of the real one; on a Quest 3 the same call cost
// 0.4 ms. A fence is asked whether it has passed instead, without ever
// sleeping in the driver.
void GlesBackend::WaitForGpu() {
    if (g_createSync) {
        using Clock = std::chrono::steady_clock;
        EGLDisplay display = static_cast<EGLDisplay>(display_);
        EGLSyncKHR fence = g_createSync(display, EGL_SYNC_FENCE_KHR, nullptr);
        if (fence != EGL_NO_SYNC_KHR) {
            const auto begin = Clock::now();
            EGLint state = g_clientWaitSync(display, fence, EGL_SYNC_FLUSH_COMMANDS_BIT_KHR, 0);
            while (state == EGL_TIMEOUT_EXPIRED_KHR) {
                const auto waited = Clock::now() - begin;
                if (waited > kWaitGiveUp) break;
                if (waited > kWaitSpin) std::this_thread::sleep_for(kWaitPoll);
                state = g_clientWaitSync(display, fence, 0, 0);
            }
            g_destroySync(display, fence);
            if (state == EGL_CONDITION_SATISFIED_KHR) return;
        }
    }
    glFinish();
}

// Waits for the particle step by asking for its result: one particle is copied
// into a buffer of its own and that buffer is read. The particle buffer itself
// is never mapped, so a driver has no reason to treat it differently.
void GlesBackend::WaitForComputeByReadback() {
    glBindBuffer(GL_COPY_READ_BUFFER, ssbo_);
    glBindBuffer(GL_COPY_WRITE_BUFFER, readbackBuffer_);
    glCopyBufferSubData(GL_COPY_READ_BUFFER, GL_COPY_WRITE_BUFFER, 0, 0, sizeof(Particle));
    if (glMapBufferRange(GL_COPY_WRITE_BUFFER, 0, sizeof(Particle), GL_MAP_READ_BIT))
        glUnmapBuffer(GL_COPY_WRITE_BUFFER);
    glBindBuffer(GL_COPY_READ_BUFFER, 0);
    glBindBuffer(GL_COPY_WRITE_BUFFER, 0);
}

// Wall-clock timing ends the particle step with WaitForGpu(), and that is only
// a measurement if the wait covers compute work. On a PowerVR Rogue GX6250
// (2026-10-08) it does not: the fence passed 0.3 ms after a dispatch whatever
// the particle count, and the step was paid for later, outside the measured
// time (a headless frame of 4,194,304 particles took 41 ms and read 0.3 ms, a
// rate of 640 GB/s). So before a run, some steps are done the way a frame does
// them, one step and one wait, and after every wait glFinish is asked whether
// anything is left. If the wait was real, glFinish returns at once. If much is
// left, every step is waited for by reading one particle back from then on.
//
// What this check must not do, each of which an earlier version did that day:
//   - Take the cost of a read-back for work left over. On an Adreno 650
//     (Quest 2) glFinish returned 0.005 ms after the wait, so nothing was left,
//     and a read-back then took 1.3 to 5.2 ms whatever the work. So glFinish is
//     the judge, and what an idle glFinish costs is taken off.
//   - Do many steps in a row before one wait. On the PowerVR only the last
//     part of such a row was left over (18 ms of 230 ms for 256 steps of 65,536
//     particles), which passed, while in a frame the one step is that last part.
void GlesBackend::CheckWaitCoversCompute() {
    if (isGpuBurnWorkload(config_.workload)) return;
    using Clock = std::chrono::steady_clock;
    const auto ms = [](Clock::time_point begin, Clock::time_point end) {
        return std::chrono::duration<double, std::milli>(end - begin).count();
    };

    glGenBuffers(1, &readbackBuffer_);
    glBindBuffer(GL_COPY_WRITE_BUFFER, readbackBuffer_);
    glBufferData(GL_COPY_WRITE_BUFFER, sizeof(Particle), nullptr, GL_DYNAMIC_READ);
    glBindBuffer(GL_COPY_WRITE_BUFFER, 0);

    const GLuint groups =
        (config_.particleCount + config_.computeWorkGroupSize - 1) / config_.computeWorkGroupSize;
    // A step of zero length leaves every particle where it is.
    const float params[4] = {0.0f, 1.0f, 0.0f, 0.0f};
    glBindBuffer(GL_UNIFORM_BUFFER, ubo_);
    glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(params), params);
    glUseProgram(computeProgram_);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, ssbo_);
    glBindBufferBase(GL_UNIFORM_BUFFER, 1, ubo_);

    constexpr int kSteps = 16;
    constexpr double kLeftFloorMs = 0.05;  // a step; less than this is the cost of asking
    glFinish();
    double waitedMs = 0.0;
    double leftMs = 0.0;
    for (int step = 0; step < kSteps; ++step) {
        const auto begin = Clock::now();
        glDispatchCompute(groups, 1, 1);
        glMemoryBarrier(GL_ALL_BARRIER_BITS);
        WaitForGpu();
        const auto waited = Clock::now();
        glFinish();
        const auto finished = Clock::now();
        glFinish();  // with nothing pending: what asking costs
        const auto idle = Clock::now();
        waitedMs += ms(begin, waited);
        leftMs += (std::max)(0.0, ms(waited, finished) - ms(finished, idle));
    }
    while (glGetError() != GL_NO_ERROR) {}

    std::cout << "[Profiling] Wait check: over " << kSteps << " particle steps the wait took " << waitedMs
              << " ms and glFinish found " << leftMs << " ms of work left after it." << std::endl;
    if (leftMs > (std::max)(kSteps * kLeftFloorMs, 0.3 * waitedMs)) {
        computeWaitByReadback_ = true;
        std::cout << "[Profiling] Waiting for the GPU does not cover compute work on this driver; under "
                     "wall-clock timing each particle step is waited for by reading one particle back."
                  << std::endl;
    }
}

// Shows the frame that was just drawn, but not every frame: a picture about
// thirty times a second. Every frame is still drawn in full and measured.
//
// A swap is not free of the display here, even at swap interval 0. On a
// Snapdragon 8 Gen 2 phone with a 120 Hz screen (2026-10-07) the first work
// after every swap was held back until the screen had refreshed, so a test
// that swapped every frame ran 120 frames a second whatever its work. Under
// wall-clock timing that wait counted as GPU time (65,536 particles read
// 0.45 GB/s); with the wait kept out of the measurement the GPU stood idle
// most of the time and the system slowed its clock (a frame of the OpenGL ES
// 2.0 GPU Burn went from 0.8 ms to 3.2 ms). Swapping this seldom, there is
// always a buffer the display let go of long ago: nothing is held back and
// the GPU stays at work, as it does under Vulkan.
void GlesBackend::Present() {
    const auto now = std::chrono::steady_clock::now();
    if (!config_.vsync && now - g_lastPresent < kPresentInterval) {
        // A swap is also what keeps the test from running ahead of the GPU.
        // Without one the frame is waited for here, or frames pile up without
        // limit: 256-step GPU Burn frames of 153 ms each, queued this way,
        // froze the phone until adb lost it (2026-10-07). Wall-clock timing
        // has waited for the frame already.
        if (!synchronizedTimingFallback_) WaitForGpu();
        return;
    }
    g_lastPresent = now;

    if (!eglSwapBuffers(static_cast<EGLDisplay>(display_), static_cast<EGLSurface>(surface_)))
        ThrowEgl("eglSwapBuffers");
    if (synchronizedTimingFallback_) {
        // Whatever the swap still holds back must not reach into the next
        // frame's time. Only real work is held, so there has to be some: a
        // clear of the new back buffer, which the frame begins with anyway.
        glClear(GL_COLOR_BUFFER_BIT);
        WaitForGpu();
    }
}

// -----------------------------------------------------------------------
// DrawFrame
// -----------------------------------------------------------------------

void GlesBackend::DrawFrame(float deltaTime) {
    if (Es2()) {
        DrawEs2BurnFrame(deltaTime);
        return;
    }

    const int slot = currentFrame_ % kTimestampSlotCount;
    using Clock = std::chrono::steady_clock;
    const auto elapsedMs = [](Clock::time_point begin, Clock::time_point end) {
        return std::chrono::duration<double, std::milli>(end - begin).count();
    };

    // Collect results from a previous frame (if ring buffer is full)
    if (UseTimestampQueries() && timestampFrameCount_ >= kTimestampSlotCount)
        CollectTimestampResults();

    const bool useQueries = UseTimestampQueries();
    const bool synchronizedTiming = synchronizedTimingFallback_;
    const auto synchronizedFrameBegin = Clock::now();

    // -- Timestamp: frame begin --
    if (useQueries)
        g_queryCounter(timestampQueries_[slot][0], kTimestamp);

    // -- GPU Burn: a fullscreen pass, no compute --
    if (isGpuBurnWorkload(config_.workload)) {
        if (useQueries)
            g_queryCounter(timestampQueries_[slot][1], kTimestamp);  // compute ~0

        glClearColor(0.0f, 0.0f, 0.02f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        if (useQueries)
            g_queryCounter(timestampQueries_[slot][2], kTimestamp);

        glUseProgram(renderProgram_);
        glBindBufferBase(GL_UNIFORM_BUFFER, 1, ubo_);
        glBindVertexArray(vao_);                 // empty fetch; VS uses gl_VertexID

        glBindBuffer(GL_UNIFORM_BUFFER, ubo_);
        burnElapsed_ += deltaTime;
        for (std::uint32_t pass = 0; pass < gpuBurnDrawsPerFrame(config_.workload); ++pass) {
            const GpuBurnParams params{ burnElapsed_, static_cast<float>(pass),
                                        config_.gpuBurnIter, gpuBurnShaderVersion(config_.workload) };
            glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(params), &params);
            glDrawArrays(GL_TRIANGLES, 0, 3);
        }

        if (useQueries)
            g_queryCounter(timestampQueries_[slot][3], kTimestamp);

        if (synchronizedTiming) {
            WaitForGpu();
            const double renderMs = elapsedMs(synchronizedFrameBegin, Clock::now());
            AccumulateTiming(0.0, renderMs, renderMs);
        }

        Present();
        ++currentFrame_;
        if (useQueries && timestampFrameCount_ < kTimestampSlotCount)
            ++timestampFrameCount_;
        return;
    }

    // -- Compute dispatch --
    // 16-byte UBO: {deltaTime, bounds, _, _}.
    const float params[4] = {deltaTime, 1.0f, 0.0f, 0.0f};
    glBindBuffer(GL_UNIFORM_BUFFER, ubo_);
    glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(params), params);

    glUseProgram(computeProgram_);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, ssbo_);
    glBindBufferBase(GL_UNIFORM_BUFFER, 1, ubo_);

    const auto synchronizedComputeBegin = Clock::now();

    const GLuint groups = (config_.particleCount + config_.computeWorkGroupSize - 1)
                          / config_.computeWorkGroupSize;
    glDispatchCompute(groups, 1, 1);

    // -- Timestamp: compute end --
    if (useQueries)
        g_queryCounter(timestampQueries_[slot][1], kTimestamp);

    double synchronizedComputeMs = 0.0;
    if (synchronizedTiming) {
        glMemoryBarrier(GL_ALL_BARRIER_BITS);
        if (computeWaitByReadback_) WaitForComputeByReadback();
        else WaitForGpu();
        synchronizedComputeMs = elapsedMs(synchronizedComputeBegin, Clock::now());
    }

    if (config_.headless) {
        // Headless: barrier for compute coherency, mirror timestamps
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
        if (useQueries) {
            g_queryCounter(timestampQueries_[slot][2], kTimestamp);
            g_queryCounter(timestampQueries_[slot][3], kTimestamp);
        }
        // Nothing is presented, so the driver is told to get on with it:
        // a full sync every 16 frames, a fence and a flush otherwise.
        if (synchronizedTiming) {
            AccumulateTiming(synchronizedComputeMs, 0.0, synchronizedComputeMs);
        } else if (currentFrame_ % 16 == 0) {
            glFinish();
        } else {
            if (frameFences_[slot])
                glDeleteSync(static_cast<GLsync>(frameFences_[slot]));
            frameFences_[slot] = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
            glFlush();
        }
    } else {
        // Barrier: ensure compute writes are visible to vertex fetch
        glMemoryBarrier(GL_VERTEX_ATTRIB_ARRAY_BARRIER_BIT);

        // -- Render --
        glClearColor(0.0f, 0.0f, 0.02f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        // -- Timestamp: render begin --
        if (useQueries)
            g_queryCounter(timestampQueries_[slot][2], kTimestamp);

        glUseProgram(renderProgram_);
        glBindVertexArray(vao_);
        glDrawArrays(GL_POINTS, 0, static_cast<GLsizei>(config_.particleCount));

        // -- Timestamp: render end --
        if (useQueries)
            g_queryCounter(timestampQueries_[slot][3], kTimestamp);

        if (synchronizedTiming) {
            WaitForGpu();
            const double totalMs = elapsedMs(synchronizedFrameBegin, Clock::now());
            const double renderMs = (std::max)(0.0, totalMs - synchronizedComputeMs);
            AccumulateTiming(synchronizedComputeMs, renderMs, totalMs);
        }

        Present();
    }

    currentFrame_++;
    if (useQueries && timestampFrameCount_ < kTimestampSlotCount)
        ++timestampFrameCount_;
}

void GlesBackend::CollectTimestampResults() {
    const int readSlot = (currentFrame_) % kTimestampSlotCount;

    // In headless mode, check the fence to see if GPU finished this slot's work.
    // glFlush alone doesn't guarantee query availability on all drivers.
    if (config_.headless && frameFences_[readSlot]) {
        GLenum res = glClientWaitSync(static_cast<GLsync>(frameFences_[readSlot]),
                                      GL_SYNC_FLUSH_COMMANDS_BIT, 0);
        if (res == GL_TIMEOUT_EXPIRED || res == GL_WAIT_FAILED)
            return;  // GPU not done yet, skip this frame's timing
        glDeleteSync(static_cast<GLsync>(frameFences_[readSlot]));
        frameFences_[readSlot] = nullptr;
    }

    GLuint available = GL_FALSE;
    glGetQueryObjectuiv(timestampQueries_[readSlot][3],
                        GL_QUERY_RESULT_AVAILABLE, &available);
    if (!available) return;

    GLuint64 ts[kTimestampsPerFrame]{};
    for (int i = 0; i < kTimestampsPerFrame; ++i)
        g_getQueryObjectui64v(timestampQueries_[readSlot][i], GL_QUERY_RESULT, &ts[i]);

    // The GPU's clock was interrupted since the flag was last read (a change of
    // power state, for one): the driver says these values cannot be trusted.
    GLint disjoint = 0;
    glGetIntegerv(kGpuDisjoint, &disjoint);
    if (disjoint) {
        if (++g_disjointSamples > kMaxDisjointSamples)
            EnableSynchronizedTimingFallback("the GPU clock keeps being interrupted");
        return;
    }
    g_disjointSamples = 0;

    if (!(ts[0] <= ts[1] && ts[1] <= ts[2] && ts[2] <= ts[3])) {
        EnableSynchronizedTimingFallback("non-monotonic query results");
        return;
    }

    double computeMs = static_cast<double>(ts[1] - ts[0]) / 1e6;
    double renderMs  = static_cast<double>(ts[3] - ts[2]) / 1e6;
    double totalMs   = static_cast<double>(ts[3] - ts[0]) / 1e6;

    // The same plausibility envelope as the desktop OpenGL backend: a stream
    // result above 10 TB/s indicates a broken timer, not a fast GPU.
    if (config_.workload == Workload::Stream && computeMs <= 0.0) {
        EnableSynchronizedTimingFallback("zero-duration stream timestamp result");
        return;
    }
    if (config_.workload == Workload::Stream) {
        constexpr double kBytesPerParticle = 40.0;
        const double apparentGbPerSec =
            (static_cast<double>(config_.particleCount) * kBytesPerParticle) /
            (computeMs / 1000.0) / 1.0e9;
        if (apparentGbPerSec > 10000.0) {
            EnableSynchronizedTimingFallback("implausible stream timestamp result");
            return;
        }
    }

    AccumulateTiming(computeMs, renderMs, totalMs);
}

// -----------------------------------------------------------------------
// Cleanup
// -----------------------------------------------------------------------

void GlesBackend::WaitIdle() {
    if (context_) glFinish();
}

void GlesBackend::CleanupBackend() {
    if (context_) {
        for (int slot = 0; slot < kTimestampSlotCount; ++slot) {
            if (frameFences_[slot]) {
                glDeleteSync(static_cast<GLsync>(frameFences_[slot]));
                frameFences_[slot] = nullptr;
            }
        }
        if (timestampQueriesAllocated_) {
            for (int slot = 0; slot < kTimestampSlotCount; ++slot)
                glDeleteQueries(kTimestampsPerFrame, timestampQueries_[slot]);
            timestampQueriesAllocated_ = false;
        }

        if (triangleVbo_)    { glDeleteBuffers(1, &triangleVbo_);    triangleVbo_ = 0; }
        if (ubo_)            { glDeleteBuffers(1, &ubo_);            ubo_ = 0; }
        if (readbackBuffer_) { glDeleteBuffers(1, &readbackBuffer_); readbackBuffer_ = 0; }
        if (vao_)            { glDeleteVertexArrays(1, &vao_);       vao_ = 0; }
        if (ssbo_)           { glDeleteBuffers(1, &ssbo_);           ssbo_ = 0; }
        if (computeProgram_) { glDeleteProgram(computeProgram_);     computeProgram_ = 0; }
        if (renderProgram_)  { glDeleteProgram(renderProgram_);      renderProgram_ = 0; }
    }
    DestroyContext();
}

}  // namespace gpu_bench

#endif  // HAVE_GLES

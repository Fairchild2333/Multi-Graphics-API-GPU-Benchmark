#include "ios_gles_backend.h"
#import <OpenGLES/EAGL.h>
#import <OpenGLES/ES2/gl.h>
#import <QuartzCore/CAEAGLLayer.h>
#include <chrono>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <vector>
#include <dispatch/dispatch.h>
namespace gpu_bench {
struct IosGlesBackend::State {
    EAGLContext* context = nil;
    GLuint framebuffer = 0, color = 0, program = 0, triangle = 0;
    GLint position = -1, phase = -1, pass = -1;
};
namespace {
GLuint Shader(GLenum type, const std::string& source) {
    const auto shader = glCreateShader(type);
    const char* text = source.c_str();
    glShaderSource(shader, 1, &text, nullptr);
    glCompileShader(shader);
    GLint ok = 0; glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        GLint length = 0; glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &length);
        std::vector<char> log(std::max(1, length));
        glGetShaderInfoLog(shader, length, nullptr, log.data());
        glDeleteShader(shader);
        throw std::runtime_error(std::string("ES 2.0 shader: ") + log.data());
    }
    return shader;
}
}
std::uint32_t IosGlesMaxSteps() {
    @autoreleasepool {
        EAGLContext* previous = [EAGLContext currentContext];
        EAGLContext* context = [[EAGLContext alloc] initWithAPI:kEAGLRenderingAPIOpenGLES2];
        if (!context || ![EAGLContext setCurrentContext:context]) return 0;
        const auto* renderer = glGetString(GL_RENDERER);
        const bool software = renderer && isSoftwareRendererName(reinterpret_cast<const char*>(renderer));
        [EAGLContext setCurrentContext:previous];
        return software ? gpuBurnMaxFixedIter(Workload::GpuBurnV1) : kGpuBurnV3MaxCustomIter;
    }
}
bool IosGlesAvailable() { return IosGlesMaxSteps() > 0; }
IosGlesBackend::~IosGlesBackend() { CleanupBackend(); }
void IosGlesBackend::InitBackend() {
    if (!window_ || config_.headless || !config_.gpuBurnEs2Legacy)
        throw std::invalid_argument("iOS OpenGL ES 2.0 requires its rendered legacy GPU Burn and CAEAGLLayer");
    state_ = new State;
    auto& state = *state_;
    state.context = [[EAGLContext alloc] initWithAPI:kEAGLRenderingAPIOpenGLES2];
    if (!state.context)
        throw std::runtime_error("EAGL context creation failed");
    auto* layer = (__bridge CAEAGLLayer*)window_;
    __block bool drawableReady = false;
    __block GLint width = 0, height = 0;
    // CAEAGLLayer's UIKit window attachment is main-thread-only. Allocate its
    // drawable there, then move the exclusively owned context to the worker.
    auto allocateDrawable = ^{
    if (![EAGLContext setCurrentContext:state.context]) return;
    glGenFramebuffers(1, &state.framebuffer);
    glGenRenderbuffers(1, &state.color);
    glBindRenderbuffer(GL_RENDERBUFFER, state.color);
    drawableReady = [state.context renderbufferStorage:GL_RENDERBUFFER fromDrawable:layer];
    glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_WIDTH, &width);
    glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_HEIGHT, &height);
    [EAGLContext setCurrentContext:nil];
    };
    if ([NSThread isMainThread]) allocateDrawable();
    else dispatch_sync(dispatch_get_main_queue(), allocateDrawable);
    if (!drawableReady || ![EAGLContext setCurrentContext:state.context])
        throw std::runtime_error("EAGL drawable allocation failed");
    if (width != kWindowWidth || height != kWindowHeight)
        throw std::runtime_error("ES 2.0 drawable must be 1280 x 720; got " + std::to_string(width) + " x " + std::to_string(height));
    renderWidth_ = width; renderHeight_ = height;
    glBindFramebuffer(GL_FRAMEBUFFER, state.framebuffer);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, state.color);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        throw std::runtime_error("EAGL framebuffer incomplete");
    const auto* renderer = glGetString(GL_RENDERER);
    const auto* version = glGetString(GL_VERSION);
    deviceName_ = renderer ? reinterpret_cast<const char*>(renderer) : "iOS OpenGL ES GPU";
    driverVersion_ = version ? reinterpret_cast<const char*>(version) : "";
    // ES 2.0 compiles its constant step count into the shader. Apply the shared
    // software-renderer cap before compilation so reported and executed work agree.
    if (isSoftwareRendererName(deviceName_))
        config_.gpuBurnIter = std::min(config_.gpuBurnIter, gpuBurnMaxFixedIter(config_.workload));
    const auto vertexBytes = ReadFileBytes(shaderDir_ + "gpu_burn_es2.vert");
    const auto fragmentBytes = ReadFileBytes(shaderDir_ + "gpu_burn_es2.frag");
    const auto vertex = Shader(GL_VERTEX_SHADER, std::string(vertexBytes.begin(), vertexBytes.end()));
    GLuint fragment;
    try { fragment = Shader(GL_FRAGMENT_SHADER, "#define BURN_STEPS " + std::to_string(config_.gpuBurnIter) + "\n" + std::string(fragmentBytes.begin(), fragmentBytes.end())); }
    catch (...) { glDeleteShader(vertex); throw; }
    state.program = glCreateProgram();
    glAttachShader(state.program, vertex); glAttachShader(state.program, fragment);
    glLinkProgram(state.program);
    glDeleteShader(vertex); glDeleteShader(fragment);
    GLint linked = 0; glGetProgramiv(state.program, GL_LINK_STATUS, &linked);
    if (!linked) throw std::runtime_error("ES 2.0 GPU Burn program failed to link");
    state.position = glGetAttribLocation(state.program, "aPosition");
    state.phase = glGetUniformLocation(state.program, "uPhase");
    state.pass = glGetUniformLocation(state.program, "uPass");
    if (state.position < 0 || state.phase < 0 || state.pass < 0)
        throw std::runtime_error("ES 2.0 GPU Burn shader contract mismatch");
    const float triangle[] = {-1, -1, 3, -1, -1, 3};
    glGenBuffers(1, &state.triangle);
    glBindBuffer(GL_ARRAY_BUFFER, state.triangle);
    glBufferData(GL_ARRAY_BUFFER, sizeof(triangle), triangle, GL_STATIC_DRAW);
    glEnable(GL_BLEND); glBlendFunc(GL_ONE, GL_ONE);
    glViewport(0, 0, width, height);
    presentFollowsDisplay_ = true;
    std::cout << "[iOS EAGL] " << deviceName_ << " | " << driverVersion_ << '\n'
              << "[Profiling] ES 2.0 legacy: synchronized wall-clock timing; display-paced presentation\n";
}
void IosGlesBackend::DrawFrame(float deltaTime) {
    auto& state = *state_;
    const auto begin = std::chrono::steady_clock::now();
    glBindFramebuffer(GL_FRAMEBUFFER, state.framebuffer);
    glClearColor(0, 0, 0, 1); glClear(GL_COLOR_BUFFER_BIT);
    glUseProgram(state.program);
    glBindBuffer(GL_ARRAY_BUFFER, state.triangle);
    glEnableVertexAttribArray(state.position);
    glVertexAttribPointer(state.position, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
    elapsed_ += deltaTime;
    glUniform1f(state.phase, std::fmod(elapsed_ * 0.7f, 6.2831853f));
    for (std::uint32_t pass = 0; pass < gpuBurnDrawsPerFrame(config_.workload); ++pass) {
        glUniform1f(state.pass, static_cast<float>(pass));
        glDrawArrays(GL_TRIANGLES, 0, 3);
    }
    glFinish();
    if (glGetError() != GL_NO_ERROR) throw std::runtime_error("ES 2.0 GPU Burn frame failed");
    const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - begin).count();
    AccumulateTiming(0, ms, ms);
    glBindRenderbuffer(GL_RENDERBUFFER, state.color);
    if (![state.context presentRenderbuffer:GL_RENDERBUFFER]) throw std::runtime_error("EAGL presentation failed");
}
void IosGlesBackend::WaitIdle() { if (state_ && state_->context) { [EAGLContext setCurrentContext:state_->context]; glFinish(); } }
void IosGlesBackend::CleanupBackend() {
    if (!state_) return;
    if ([EAGLContext setCurrentContext:state_->context]) {
        glFinish();
        if (state_->program) glDeleteProgram(state_->program);
        if (state_->triangle) glDeleteBuffers(1, &state_->triangle);
        if (state_->color) glDeleteRenderbuffers(1, &state_->color);
        if (state_->framebuffer) glDeleteFramebuffers(1, &state_->framebuffer);
        [EAGLContext setCurrentContext:nil];
    }
    delete state_; state_ = nullptr;
}
}

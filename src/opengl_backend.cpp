#ifdef HAVE_OPENGL

#include "opengl_backend.h"
#include "mini_mat.h"

#include <glad/gl.h>
#include <GLFW/glfw3.h>

#include <cstdint>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace gpu_bench {

// -----------------------------------------------------------------------
// Shader helpers
// -----------------------------------------------------------------------

std::uint32_t OpenGLBackend::CompileShaderGL(const std::string& path, std::uint32_t type) {
    auto src = ReadFileBytes(path);
    src.push_back('\0');

    GLuint shader = glCreateShader(type);
    const char* srcPtr = src.data();
    glShaderSource(shader, 1, &srcPtr, nullptr);
    glCompileShader(shader);

    GLint ok = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[1024]{};
        glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
        glDeleteShader(shader);
        throw std::runtime_error("Shader compilation failed (" + path + "):\n" + log);
    }
    return shader;
}

std::uint32_t OpenGLBackend::LinkProgramGL(std::uint32_t s1, std::uint32_t s2) {
    GLuint prog = glCreateProgram();
    glAttachShader(prog, s1);
    if (s2) glAttachShader(prog, s2);
    glLinkProgram(prog);

    GLint ok = GL_FALSE;
    glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[1024]{};
        glGetProgramInfoLog(prog, sizeof(log), nullptr, log);
        glDeleteProgram(prog);
        throw std::runtime_error(std::string("Program link failed:\n") + log);
    }

    glDetachShader(prog, s1);
    glDeleteShader(s1);
    if (s2) { glDetachShader(prog, s2); glDeleteShader(s2); }
    return prog;
}

// -----------------------------------------------------------------------
// Init
// -----------------------------------------------------------------------

void OpenGLBackend::InitBackend() {
    std::cout << "[OpenGL Init] Loading GL functions..." << std::endl;
    glfwMakeContextCurrent(window_);
    int gladVer = gladLoadGL(glfwGetProcAddress);
    if (!gladVer)
        throw std::runtime_error("Failed to initialise GLAD");
    std::cout << "[OpenGL Init] GLAD loaded GL "
              << GLAD_VERSION_MAJOR(gladVer) << "."
              << GLAD_VERSION_MINOR(gladVer) << std::endl;

    deviceName_ = reinterpret_cast<const char*>(glGetString(GL_RENDERER));
    std::string glVerStr = reinterpret_cast<const char*>(glGetString(GL_VERSION));
    driverVersion_ = glVerStr;
    std::cout << "[OpenGL Init] " << deviceName_ << "  |  GL " << glVerStr << std::endl;

    if (GLAD_VERSION_MAJOR(gladVer) < 4 ||
        (GLAD_VERSION_MAJOR(gladVer) == 4 && GLAD_VERSION_MINOR(gladVer) < 3)) {
        throw std::runtime_error("OpenGL 4.3+ required for compute shaders");
    }

    if (config_.vsync)
        glfwSwapInterval(1);
    else
        glfwSwapInterval(0);

    std::cout << "[OpenGL Init] Compiling shaders..." << std::endl;
    CreateShaders();
    std::cout << "[OpenGL Init] Creating particle buffers..." << std::endl;
    CreateParticleBuffers();
    std::cout << "[OpenGL Init] Creating timestamp queries..." << std::endl;
    CreateTimestampQueries();

    if (!config_.headless) {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glEnable(GL_PROGRAM_POINT_SIZE);
        glViewport(0, 0, static_cast<GLsizei>(kWindowWidth),
                   static_cast<GLsizei>(kWindowHeight));
    }

    std::cout << "[OpenGL Init] Initialisation complete." << std::endl;
}

void OpenGLBackend::CreateShaders() {
    {
        std::string csFile;
        if (config_.workload == Workload::SynthPeak) {
            if (config_.peakPrecision == Precision::FP16) {
                // Desktop GL has no portable FP16; use the NVIDIA extension when present.
                if (!GLAD_GL_NV_gpu_shader5)
                    throw std::runtime_error("SynthPeak FP16 on OpenGL requires GL_NV_gpu_shader5 (NVIDIA); use Vulkan/Metal elsewhere");
                csFile = "synthpeak_fp16_gl.comp";
            } else {
                csFile = (config_.peakPrecision == Precision::FP64)  ? "synthpeak_fp64_gl.comp"
                       : (config_.peakPrecision == Precision::INT32) ? "synthpeak_int32_gl.comp"
                       :                                               "synthpeak_fp32_gl.comp";
            }
        } else {
            csFile = (config_.workload == Workload::NBody) ? "nbody_gl.comp" : "compute_gl.comp";
        }
        auto cs = CompileShaderGL(shaderDir_ + csFile, GL_COMPUTE_SHADER);
        computeProgram_ = LinkProgramGL(cs, 0);
    }
    if (!config_.headless) {
        const char* vsf = "particle_gl.vert";
        const char* fsf = "particle_gl.frag";
        if (config_.workload == Workload::StressFractal) { vsf = "fractal_gl.vert";  fsf = "fractal_gl.frag"; }
        else if (config_.workload == Workload::Volumetric) { vsf = "volumetric_gl.vert"; fsf = "volumetric_gl.frag"; }
        else if (config_.workload == Workload::Render3D) { vsf = "render3d_gl.vert"; fsf = "render3d_gl.frag"; }
        auto vs = CompileShaderGL(shaderDir_ + vsf, GL_VERTEX_SHADER);
        auto fs = CompileShaderGL(shaderDir_ + fsf, GL_FRAGMENT_SHADER);
        renderProgram_ = LinkProgramGL(vs, fs);
    }
}

void OpenGLBackend::CreateParticleBuffers() {
    const GLsizeiptr bufferSize =
        static_cast<GLsizeiptr>(sizeof(Particle) * config_.particleCount);

    glGenBuffers(1, &ssbo_);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo_);

    if (config_.hostMemory) {
        GLbitfield flags = GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT;
        glBufferStorage(GL_SHADER_STORAGE_BUFFER, bufferSize,
                        initialParticles_.data(), flags);
    } else {
        glBufferStorage(GL_SHADER_STORAGE_BUFFER, bufferSize,
                        initialParticles_.data(), 0);
    }

    std::cout << "Created particle buffers: " << config_.particleCount
              << " particles" << std::endl;

    // VAO — bind the same SSBO as vertex buffer (not needed in headless)
    if (!config_.headless) {
        glGenVertexArrays(1, &vao_);
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

    // UBO for compute params (deltaTime + bounds, std140 padded to 16 bytes)
    glGenBuffers(1, &ubo_);
    glBindBuffer(GL_UNIFORM_BUFFER, ubo_);
    float params[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    glBufferData(GL_UNIFORM_BUFFER, sizeof(params), params, GL_DYNAMIC_DRAW);

    // Render3D: quad VBO + instanced VAO + camera UBO (default FBO supplies depth)
    if (config_.workload == Workload::Render3D && !config_.headless) {
        const float quad[12] = { -1,-1, 1,-1, 1,1, -1,-1, 1,1, -1,1 };
        glGenBuffers(1, &quadVbo_);
        glBindBuffer(GL_ARRAY_BUFFER, quadVbo_);
        glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);

        glGenVertexArrays(1, &render3dVao_);
        glBindVertexArray(render3dVao_);
        // location 0 = quad corner (per-vertex)
        glBindBuffer(GL_ARRAY_BUFFER, quadVbo_);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 2, reinterpret_cast<void*>(0));
        // location 1/2 = particle position/velocity (per-instance)
        glBindBuffer(GL_ARRAY_BUFFER, ssbo_);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(Particle), reinterpret_cast<void*>(0));
        glVertexAttribDivisor(1, 1);
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(Particle),
                              reinterpret_cast<void*>(offsetof(Particle, vx)));
        glVertexAttribDivisor(2, 1);
        glBindVertexArray(0);

        glGenBuffers(1, &cam3dUbo_);
        glBindBuffer(GL_UNIFORM_BUFFER, cam3dUbo_);
        glBufferData(GL_UNIFORM_BUFFER, sizeof(Render3DParams), nullptr, GL_DYNAMIC_DRAW);
    }
}

void OpenGLBackend::CreateTimestampQueries() {
    GLint bits = 0;
    glGetQueryiv(GL_TIMESTAMP, GL_QUERY_COUNTER_BITS, &bits);
    timestampsSupported_ = (bits > 0);

    if (!timestampsSupported_) {
        std::cout << "[Profiling] OpenGL timestamp queries not supported." << std::endl;
        return;
    }

    for (int s = 0; s < kTimestampSlotCount; ++s)
        glGenQueries(kTimestampsPerFrame, timestampQueries_[s]);

    std::cout << "[Profiling] OpenGL timestamp queries enabled." << std::endl;
}

// -----------------------------------------------------------------------
// DrawFrame
// -----------------------------------------------------------------------

void OpenGLBackend::DrawFrame(float deltaTime) {
    const int slot = currentFrame_ % kTimestampSlotCount;

    // Collect results from a previous frame (if ring buffer is full)
    if (timestampsSupported_ && timestampFrameCount_ >= kTimestampSlotCount)
        CollectTimestampResults();

    // -- Timestamp: frame begin --
    if (timestampsSupported_)
        glQueryCounter(timestampQueries_[slot][0], GL_TIMESTAMP);

    // -- Fragment-only fullscreen pass (fractal / volumetric): no compute --
    if (config_.workload == Workload::StressFractal
        || config_.workload == Workload::Volumetric) {
        if (timestampsSupported_)
            glQueryCounter(timestampQueries_[slot][1], GL_TIMESTAMP);  // compute ~0

        glClearColor(0.0f, 0.0f, 0.02f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        if (timestampsSupported_)
            glQueryCounter(timestampQueries_[slot][2], GL_TIMESTAMP);

        // 16-byte UBO layout (matches both FractalParams and VolumetricParams):
        //   { time(f), scalar(f), count(u32), _ }
        unsigned char pb[16] = {0};
        fractalElapsed_ += deltaTime;
        float fp[2];
        std::uint32_t count;
        if (config_.workload == Workload::Volumetric) {
            fp[0] = fractalElapsed_; fp[1] = 0.05f;       // stepSize
            count = config_.volumetricSteps;
        } else {
            fp[0] = fractalElapsed_; fp[1] = 1.0f;        // zoom
            count = config_.fractalIter;
        }
        std::memcpy(pb,        fp,    sizeof(fp));
        std::memcpy(pb + 8,    &count, sizeof(count));
        glBindBuffer(GL_UNIFORM_BUFFER, ubo_);
        glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(pb), pb);

        glUseProgram(renderProgram_);
        glBindBufferBase(GL_UNIFORM_BUFFER, 1, ubo_);
        glBindVertexArray(vao_);                 // empty fetch; VS uses gl_VertexID
        glDrawArrays(GL_TRIANGLES, 0, 3);

        if (timestampsSupported_)
            glQueryCounter(timestampQueries_[slot][3], GL_TIMESTAMP);

        glfwSwapBuffers(window_);
        ++currentFrame_;
        if (timestampsSupported_ && timestampFrameCount_ < kTimestampSlotCount)
            ++timestampFrameCount_;
        return;
    }

    // -- Compute dispatch --
    // 16-byte UBO. Stream: {deltaTime, bounds, _, _}.
    // N-body: {deltaTime, softening, numBodies(uint bits), _}.
    unsigned char paramBytes[16] = {0};
    if (config_.workload == Workload::SynthPeak) {
        // {iters(uint), mul, add}
        std::memcpy(paramBytes, &config_.peakIters, sizeof(std::uint32_t));
        float fparams[2] = {0.9999f, 0.0001f};
        std::memcpy(paramBytes + 4, fparams, sizeof(fparams));
    } else if (config_.workload == Workload::NBody) {
        float fparams[2] = {deltaTime, config_.softening};
        std::memcpy(paramBytes, fparams, sizeof(fparams));
        std::memcpy(paramBytes + 8, &config_.particleCount, sizeof(std::uint32_t));
    } else {
        float fparams[4] = {deltaTime, 1.0f, 0.0f, 0.0f};
        std::memcpy(paramBytes, fparams, sizeof(fparams));
    }
    glBindBuffer(GL_UNIFORM_BUFFER, ubo_);
    glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(paramBytes), paramBytes);

    glUseProgram(computeProgram_);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, ssbo_);
    glBindBufferBase(GL_UNIFORM_BUFFER, 1, ubo_);

    GLuint groups = (config_.particleCount + kComputeWorkGroupSize - 1)
                    / kComputeWorkGroupSize;
    glDispatchCompute(groups, 1, 1);

    // -- Timestamp: compute end --
    if (timestampsSupported_)
        glQueryCounter(timestampQueries_[slot][1], GL_TIMESTAMP);

    if (config_.headless) {
        // Headless: barrier for compute coherency, mirror timestamps
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
        if (timestampsSupported_) {
            glQueryCounter(timestampQueries_[slot][2], GL_TIMESTAMP);
            glQueryCounter(timestampQueries_[slot][3], GL_TIMESTAMP);
        }
        // Periodic glFinish to force AMD driver to process commands on
        // hidden windows (glFlush alone is insufficient on some drivers).
        // Every 16 frames: full sync; otherwise: fence + flush.
        if (currentFrame_ % 16 == 0) {
            glFinish();
        } else {
            if (frameFences_[slot])
                glDeleteSync(static_cast<GLsync>(frameFences_[slot]));
            frameFences_[slot] = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
            glFlush();
        }
    } else if (config_.workload == Workload::Render3D) {
        // Barrier: compute writes visible to vertex fetch
        glMemoryBarrier(GL_VERTEX_ATTRIB_ARRAY_BARRIER_BIT);

        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LESS);
        glClearColor(0.04f, 0.08f, 0.14f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        if (timestampsSupported_)
            glQueryCounter(timestampQueries_[slot][2], GL_TIMESTAMP);

        fractalElapsed_ += deltaTime;
        Render3DParams r3{};
        const float aspect = static_cast<float>(kWindowWidth) / static_cast<float>(kWindowHeight);
        render3dCamera(fractalElapsed_, aspect, /*flipY*/false, /*z01*/false,
                       r3.viewProj, r3.camRight, r3.camUp);
        r3.pointSize = 0.02f;
        glBindBuffer(GL_UNIFORM_BUFFER, cam3dUbo_);
        glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(Render3DParams), &r3);

        glUseProgram(renderProgram_);
        glBindBufferBase(GL_UNIFORM_BUFFER, 0, cam3dUbo_);
        glBindVertexArray(render3dVao_);
        glDrawArraysInstanced(GL_TRIANGLES, 0, 6, static_cast<GLsizei>(config_.particleCount));

        if (timestampsSupported_)
            glQueryCounter(timestampQueries_[slot][3], GL_TIMESTAMP);

        glDisable(GL_DEPTH_TEST);
        glfwSwapBuffers(window_);
        ++currentFrame_;
        if (timestampsSupported_ && timestampFrameCount_ < kTimestampSlotCount)
            ++timestampFrameCount_;
        return;
    } else {
        // Barrier: ensure compute writes are visible to vertex fetch
        glMemoryBarrier(GL_VERTEX_ATTRIB_ARRAY_BARRIER_BIT);

        // -- Render --
        glClearColor(0.0f, 0.0f, 0.02f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        // -- Timestamp: render begin --
        if (timestampsSupported_)
            glQueryCounter(timestampQueries_[slot][2], GL_TIMESTAMP);

        glUseProgram(renderProgram_);
        glBindVertexArray(vao_);
        glDrawArrays(GL_POINTS, 0, static_cast<GLsizei>(config_.particleCount));

        // -- Timestamp: render end --
        if (timestampsSupported_)
            glQueryCounter(timestampQueries_[slot][3], GL_TIMESTAMP);

        glfwSwapBuffers(window_);
    }

    currentFrame_++;
    if (timestampsSupported_ && timestampFrameCount_ < kTimestampSlotCount)
        ++timestampFrameCount_;
}

void OpenGLBackend::CollectTimestampResults() {
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

    GLint available = GL_FALSE;
    glGetQueryObjectiv(timestampQueries_[readSlot][3],
                       GL_QUERY_RESULT_AVAILABLE, &available);
    if (!available) return;

    GLuint64 ts[kTimestampsPerFrame]{};
    for (int i = 0; i < kTimestampsPerFrame; ++i)
        glGetQueryObjectui64v(timestampQueries_[readSlot][i],
                              GL_QUERY_RESULT, &ts[i]);

    double computeMs = static_cast<double>(ts[1] - ts[0]) / 1e6;
    double renderMs  = static_cast<double>(ts[3] - ts[2]) / 1e6;
    double totalMs   = static_cast<double>(ts[3] - ts[0]) / 1e6;

    if (computeMs >= 0.0 && renderMs >= 0.0 && totalMs >= 0.0)
        AccumulateTiming(computeMs, renderMs, totalMs);
}

// -----------------------------------------------------------------------
// Cleanup
// -----------------------------------------------------------------------

void OpenGLBackend::CleanupBackend() {
    for (int s = 0; s < kTimestampSlotCount; ++s) {
        if (frameFences_[s]) {
            glDeleteSync(static_cast<GLsync>(frameFences_[s]));
            frameFences_[s] = nullptr;
        }
    }
    if (timestampsSupported_) {
        for (int s = 0; s < kTimestampSlotCount; ++s)
            glDeleteQueries(kTimestampsPerFrame, timestampQueries_[s]);
    }

    if (ubo_)            { glDeleteBuffers(1, &ubo_);            ubo_ = 0; }
    if (vao_)            { glDeleteVertexArrays(1, &vao_);       vao_ = 0; }
    if (ssbo_)           { glDeleteBuffers(1, &ssbo_);           ssbo_ = 0; }
    if (computeProgram_) { glDeleteProgram(computeProgram_);     computeProgram_ = 0; }
    if (renderProgram_)  { glDeleteProgram(renderProgram_);      renderProgram_ = 0; }
}

void OpenGLBackend::WaitIdle() {
    glFinish();
}

}  // namespace gpu_bench

#endif  // HAVE_OPENGL

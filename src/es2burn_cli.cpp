// Stand-alone OpenGL ES 2.0 GPU Burn (gpu_burn_es2_legacy_v1), for a device the
// app cannot be installed on: Android older than 6.0, reached over adb. It
// draws the frames of GlesBackend::DrawEs2BurnFrame into an off-screen buffer of
// the same size, times them the same way (wall clock, ended by a fence) and
// reports the rate by the same formula. Nothing is shown and nothing is saved.
// Build it with scripts/build-android-cpumem.ps1.
//
// The two shader files (gpu_burn_es2.vert, gpu_burn_es2.frag) are read from
// --shader-dir, the current directory by default.
//
// Only the C library is used, and the program is linked without the C++
// library: the NDK's C++ library needs functions of the C library (strtof,
// strtold and more) that Android 4.4 does not have, and one std::string was
// enough to keep the program from starting there. atof is missing there too.

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES2/gl2.h>

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

namespace {

// As in the engine: the render size of every mobile GPU test, two draws a
// frame, and how a wait asks the fence (gles_backend.cpp).
const int kWidth = 1280;
const int kHeight = 720;
const int kDrawsPerFrame = 2;
const double kWarmupSeconds = 2.0;
const double kWaitSpinSeconds = 0.001;
const long kWaitPollNanoseconds = 50000;
const double kWaitGiveUpSeconds = 5.0;

typedef EGLSyncKHR (*CreateSyncFn)(EGLDisplay, EGLenum, const EGLint*);
typedef EGLBoolean (*DestroySyncFn)(EGLDisplay, EGLSyncKHR);
typedef EGLint (*ClientWaitSyncFn)(EGLDisplay, EGLSyncKHR, EGLint, EGLTimeKHR);
CreateSyncFn g_createSync = nullptr;
DestroySyncFn g_destroySync = nullptr;
ClientWaitSyncFn g_clientWaitSync = nullptr;
bool g_waitByReadback = false;

double Now() {
    timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    return static_cast<double>(now.tv_sec) + static_cast<double>(now.tv_nsec) * 1e-9;
}

void WaitForGpu(EGLDisplay display) {
    if (g_waitByReadback) {
        // A pixel read back cannot arrive before the frame is drawn.
        unsigned char pixel[4];
        glReadPixels(0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel);
        return;
    }
    if (g_createSync) {
        EGLSyncKHR fence = g_createSync(display, EGL_SYNC_FENCE_KHR, nullptr);
        if (fence != EGL_NO_SYNC_KHR) {
            const double begin = Now();
            EGLint state = g_clientWaitSync(display, fence, EGL_SYNC_FLUSH_COMMANDS_BIT_KHR, 0);
            while (state == EGL_TIMEOUT_EXPIRED_KHR) {
                const double waited = Now() - begin;
                if (waited > kWaitGiveUpSeconds) break;
                if (waited > kWaitSpinSeconds) {
                    const timespec pause = {0, kWaitPollNanoseconds};
                    nanosleep(&pause, nullptr);
                }
                state = g_clientWaitSync(display, fence, 0, 0);
            }
            g_destroySync(display, fence);
            if (state == EGL_CONDITION_SATISFIED_KHR) return;
        }
    }
    glFinish();
}

// The file `name` in `directory`, after `prefix`, as one string from malloc.
char* ReadShader(const char* directory, const char* name, const char* prefix) {
    char path[1024];
    snprintf(path, sizeof(path), "%s/%s", directory, name);
    FILE* file = fopen(path, "rb");
    if (!file) return nullptr;
    const size_t prefixLength = strlen(prefix);
    size_t capacity = prefixLength + 65536;
    size_t length = prefixLength;
    char* text = static_cast<char*>(malloc(capacity));
    if (text) {
        memcpy(text, prefix, prefixLength);
        size_t got = 0;
        while ((got = fread(text + length, 1, capacity - length - 1, file)) > 0) {
            length += got;
            if (capacity - length < 4096) {
                capacity *= 2;
                char* larger = static_cast<char*>(realloc(text, capacity));
                if (!larger) { free(text); text = nullptr; break; }
                text = larger;
            }
        }
        if (text) text[length] = '\0';
    }
    fclose(file);
    return text;
}

GLuint Compile(GLenum type, const char* source, const char* name) {
    const GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);
    GLint ok = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (ok != GL_TRUE) {
        char log[2048] = {};
        glGetShaderInfoLog(shader, sizeof(log) - 1, nullptr, log);
        fprintf(stderr, "%s did not compile:\n%s\n", name, log);
        return 0;
    }
    return shader;
}

void PrintUsage() {
    fprintf(stderr, "Usage: mangekyo_es2burn [--steps 16|64|256] [--seconds <s>] [--shader-dir <path>]\n");
}

}  // namespace

int main(int argc, char* argv[]) {
    int steps = 16;
    double seconds = 15.0;
    const char* shaderDir = ".";
    for (int i = 1; i < argc; ++i) {
        const bool hasValue = i + 1 < argc;
        if (strcmp(argv[i], "--steps") == 0 && hasValue) {
            steps = static_cast<int>(strtol(argv[++i], nullptr, 10));
        } else if (strcmp(argv[i], "--seconds") == 0 && hasValue) {
            seconds = strtod(argv[++i], nullptr);
        } else if (strcmp(argv[i], "--shader-dir") == 0 && hasValue) {
            shaderDir = argv[++i];
        } else {
            PrintUsage();
            return 2;
        }
    }
    if (steps < 1 || steps > 4096 || seconds < 3.0 || seconds > 600.0) {
        PrintUsage();
        return 2;
    }

    EGLDisplay display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (display == EGL_NO_DISPLAY || !eglInitialize(display, nullptr, nullptr)) {
        fprintf(stderr, "eglInitialize failed (EGL error 0x%x)\n", eglGetError());
        return 1;
    }
    const EGLint configAttributes[] = {EGL_SURFACE_TYPE, EGL_PBUFFER_BIT,
                                       EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
                                       EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8,
                                       EGL_NONE};
    EGLConfig config = nullptr;
    EGLint count = 0;
    if (!eglChooseConfig(display, configAttributes, &config, 1, &count) || count < 1) {
        fprintf(stderr, "This device offers no off-screen OpenGL ES 2.0 surface.\n");
        return 1;
    }
    const EGLint surfaceAttributes[] = {EGL_WIDTH, kWidth, EGL_HEIGHT, kHeight, EGL_NONE};
    const EGLint contextAttributes[] = {EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE};
    EGLSurface surface = eglCreatePbufferSurface(display, config, surfaceAttributes);
    EGLContext context = eglCreateContext(display, config, EGL_NO_CONTEXT, contextAttributes);
    if (surface == EGL_NO_SURFACE || context == EGL_NO_CONTEXT ||
        !eglMakeCurrent(display, surface, surface, context)) {
        fprintf(stderr, "The off-screen surface or the context could not be made (EGL error 0x%x)\n",
                eglGetError());
        return 1;
    }
    const char* renderer = reinterpret_cast<const char*>(glGetString(GL_RENDERER));
    printf("[OpenGL ES Init] %s  |  %s\n", renderer,
           reinterpret_cast<const char*>(glGetString(GL_VERSION)));
    // Zhaoxin's driver (21.00.93 on a KX-6000 C-960) hangs the GPU whenever it
    // draws into a pbuffer, and its EGL fences can report a frame done long
    // before the GPU is (single frames read 0.03 ms and 2.8 s); glFinish
    // holds for frames of 64 ms but returned at once on some frames of a few
    // hundred ms. On that driver the frames go into a texture of the same
    // size and every frame is waited for by reading one pixel back.
    const bool zhaoxin = renderer && strncmp(renderer, "ZX ", 3) == 0;

    const char* extensions = eglQueryString(display, EGL_EXTENSIONS);
    if (extensions && strstr(extensions, "EGL_KHR_fence_sync")) {
        g_createSync = reinterpret_cast<CreateSyncFn>(eglGetProcAddress("eglCreateSyncKHR"));
        g_destroySync = reinterpret_cast<DestroySyncFn>(eglGetProcAddress("eglDestroySyncKHR"));
        g_clientWaitSync = reinterpret_cast<ClientWaitSyncFn>(eglGetProcAddress("eglClientWaitSyncKHR"));
    }
    if (zhaoxin) {
        g_createSync = nullptr;
        g_waitByReadback = true;
        printf("[Profiling] Zhaoxin driver: its EGL fences and glFinish can return before the GPU "
               "is done; waiting for every frame by reading one pixel back.\n");
    } else if (!g_createSync || !g_destroySync || !g_clientWaitSync) {
        g_createSync = nullptr;
        printf("[Profiling] No EGL fence on this device: waiting for the GPU with glFinish, "
               "which may round a wait up to the system's timer tick.\n");
    }

    // GLSL ES 1.00 wants a constant loop bound, so the step count is compiled in.
    char define[48];
    snprintf(define, sizeof(define), "#define BURN_STEPS %d\n", steps);
    char* vertexSource = ReadShader(shaderDir, "gpu_burn_es2.vert", "");
    char* fragmentSource = ReadShader(shaderDir, "gpu_burn_es2.frag", define);
    if (!vertexSource || !fragmentSource) {
        fprintf(stderr, "gpu_burn_es2.vert and gpu_burn_es2.frag must be in %s\n", shaderDir);
        return 1;
    }
    const GLuint vertex = Compile(GL_VERTEX_SHADER, vertexSource, "gpu_burn_es2.vert");
    const GLuint fragment = Compile(GL_FRAGMENT_SHADER, fragmentSource, "gpu_burn_es2.frag");
    free(vertexSource);
    free(fragmentSource);
    if (!vertex || !fragment) return 1;
    const GLuint program = glCreateProgram();
    glAttachShader(program, vertex);
    glAttachShader(program, fragment);
    glLinkProgram(program);
    GLint linked = GL_FALSE;
    glGetProgramiv(program, GL_LINK_STATUS, &linked);
    if (linked != GL_TRUE) {
        char log[2048] = {};
        glGetProgramInfoLog(program, sizeof(log) - 1, nullptr, log);
        fprintf(stderr, "The GPU Burn program did not link:\n%s\n", log);
        return 1;
    }
    const GLint positionSlot = glGetAttribLocation(program, "aPosition");
    const GLint phaseUniform = glGetUniformLocation(program, "uPhase");
    const GLint passUniform = glGetUniformLocation(program, "uPass");
    if (positionSlot < 0) {
        fprintf(stderr, "gpu_burn_es2.vert has no attribute aPosition\n");
        return 1;
    }

    // One triangle that covers the screen; the two draws of a frame add up.
    const float corners[6] = {-1.0f, -1.0f, 3.0f, -1.0f, -1.0f, 3.0f};
    GLuint triangle = 0;
    glGenBuffers(1, &triangle);
    glBindBuffer(GL_ARRAY_BUFFER, triangle);
    glBufferData(GL_ARRAY_BUFFER, sizeof(corners), corners, GL_STATIC_DRAW);
    if (zhaoxin) {
        GLuint texture = 0, framebuffer = 0;
        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_2D, texture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, kWidth, kHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        glGenFramebuffers(1, &framebuffer);
        glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);
        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
            fprintf(stderr, "The %dx%d texture could not be drawn into.\n", kWidth, kHeight);
            return 1;
        }
        printf("[Zhaoxin] Drawing into a %dx%d texture: the driver hangs the GPU when it draws "
               "into a pbuffer.\n", kWidth, kHeight);
    }
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE);
    glViewport(0, 0, kWidth, kHeight);

    const double runBegin = Now();
    double previousFrame = runBegin;
    float elapsed = 0.0f;
    unsigned frames = 0;
    double sumMs = 0.0, minMs = 0.0, maxMs = 0.0;
    while (true) {
        const double frameBegin = Now();
        if (frameBegin - runBegin >= seconds) break;
        const float deltaTime = static_cast<float>(frameBegin - previousFrame);
        previousFrame = frameBegin;

        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        glUseProgram(program);
        glBindBuffer(GL_ARRAY_BUFFER, triangle);
        glEnableVertexAttribArray(static_cast<GLuint>(positionSlot));
        glVertexAttribPointer(static_cast<GLuint>(positionSlot), 2, GL_FLOAT, GL_FALSE, 0, nullptr);
        // The shader works in mediump: the phase is kept inside one turn here.
        elapsed += deltaTime;
        glUniform1f(phaseUniform, fmodf(elapsed * 0.7f, 6.2831853f));
        for (int pass = 0; pass < kDrawsPerFrame; ++pass) {
            glUniform1f(passUniform, static_cast<float>(pass));
            glDrawArrays(GL_TRIANGLES, 0, 3);
        }
        WaitForGpu(display);
        const double ms = (Now() - frameBegin) * 1000.0;
        if (frameBegin - runBegin >= kWarmupSeconds) {
            if (frames == 0 || ms < minMs) minMs = ms;
            if (frames == 0 || ms > maxMs) maxMs = ms;
            sumMs += ms;
            ++frames;
        }
    }

    // The middle of the last frame, as proof that something was drawn.
    unsigned char pixel[4] = {};
    glReadPixels(kWidth / 2, kHeight / 2, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel);
    const GLenum error = glGetError();

    if (frames == 0) {
        fprintf(stderr, "No frame was measured in %.1f s.\n", seconds);
        return 1;
    }
    const double average = sumMs / frames;
    const double pixels = static_cast<double>(kWidth) * kHeight;
    const double rate = pixels * steps * kDrawsPerFrame / (average / 1000.0) / 1e9;

    printf("GPU Burn:     ES 2.0 legacy v1 quasicrystal plasma, %d draws x %d fixed steps/pixel  (%dx%d px)\n",
           kDrawsPerFrame, steps, kWidth, kHeight);
    printf("Off-screen:   drawn into a buffer of that size; nothing is shown, nothing is saved\n");
    printf("Duration:     %.1f s (warmup: %.1f s, measured: %u frames)\n", seconds - kWarmupSeconds,
           kWarmupSeconds, frames);
    printf("Render ms:    avg %.3f, min %.3f, max %.3f\n", average, minMs, maxMs);
    printf("Last frame:   centre pixel %d %d %d, GL error 0x%x\n", pixel[0], pixel[1], pixel[2], error);
    printf("Burn rate:    %.2f Gpix-step/s  (%.3f ms render)\n", rate, average);
    return 0;
}

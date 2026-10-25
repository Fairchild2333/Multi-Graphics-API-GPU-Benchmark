/* What the Xorg modesetting driver's glamor would get: EGL on GBM on a display
 * card (default /dev/dri/card0). Prints the renderer, clears a scanout-capable
 * GBM surface to red, draws a green triangle, and reads both back. */
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES2/gl2.h>
#include <fcntl.h>
#include <gbm.h>
#include <stdio.h>

static void show(const char *what)
{
    unsigned char p[4] = {0};
    glReadPixels(32, 32, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, p);
    printf("%-10s pixel %3d %3d %3d %3d  GL error 0x%x\n", what, p[0], p[1], p[2], p[3], glGetError());
}

int main(int argc, char **argv)
{
    const char *card = argc > 1 ? argv[1] : "/dev/dri/card0";
    int fd = open(card, O_RDWR | O_CLOEXEC);
    if (fd < 0) { perror(card); return 1; }
    struct gbm_device *gbm = gbm_create_device(fd);
    printf("gbm backend: %s\n", gbm ? gbm_device_get_backend_name(gbm) : "(none)");
    if (!gbm) return 1;

    EGLDisplay d = eglGetPlatformDisplay(EGL_PLATFORM_GBM_KHR, gbm, NULL);
    if (!eglInitialize(d, NULL, NULL)) { printf("eglInitialize failed 0x%x\n", eglGetError()); return 1; }
    const EGLint ca[] = {EGL_SURFACE_TYPE, EGL_WINDOW_BIT, EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
                         EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8, EGL_ALPHA_SIZE, 0,
                         EGL_NATIVE_VISUAL_ID, GBM_FORMAT_XRGB8888, EGL_NONE};
    EGLConfig cfgs[64];
    EGLint n = 0;
    eglChooseConfig(d, ca, cfgs, 64, &n);
    EGLConfig c = NULL;
    for (int i = 0; i < n && !c; i++) {
        EGLint id = 0;
        eglGetConfigAttrib(d, cfgs[i], EGL_NATIVE_VISUAL_ID, &id);
        if (id == GBM_FORMAT_XRGB8888) c = cfgs[i];
    }
    if (!c) { printf("no XRGB8888 config (%d configs)\n", n); return 1; }

    struct gbm_surface *gs = gbm_surface_create(gbm, 64, 64, GBM_FORMAT_XRGB8888,
                                                GBM_BO_USE_SCANOUT | GBM_BO_USE_RENDERING);
    if (!gs) { printf("gbm_surface_create failed\n"); return 1; }
    const EGLint xa[] = {EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE};
    EGLSurface s = eglCreatePlatformWindowSurface(d, c, gs, NULL);
    EGLContext x = eglCreateContext(d, c, EGL_NO_CONTEXT, xa);
    if (s == EGL_NO_SURFACE || x == EGL_NO_CONTEXT || !eglMakeCurrent(d, s, s, x)) {
        printf("surface/context failed 0x%x\n", eglGetError());
        return 1;
    }
    printf("renderer: %s\n", glGetString(GL_RENDERER));

    glClearColor(1, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    show("clear red");

    const char *vs = "attribute vec2 p; void main() { gl_Position = vec4(p, 0.0, 1.0); }";
    const char *fs = "precision mediump float; void main() { gl_FragColor = vec4(0.0, 1.0, 0.0, 1.0); }";
    GLuint v = glCreateShader(GL_VERTEX_SHADER), f = glCreateShader(GL_FRAGMENT_SHADER), prog = glCreateProgram();
    glShaderSource(v, 1, &vs, NULL); glCompileShader(v);
    glShaderSource(f, 1, &fs, NULL); glCompileShader(f);
    glAttachShader(prog, v); glAttachShader(prog, f);
    glBindAttribLocation(prog, 0, "p");
    glLinkProgram(prog);
    glUseProgram(prog);
    const float tri[] = {-1, -1, 3, -1, -1, 3};
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, tri);
    glEnableVertexAttribArray(0);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    show("draw green");

    /* What a compositor does next: swap, then lock the front buffer for scanout. */
    eglSwapBuffers(d, s);
    struct gbm_bo *bo = gbm_surface_lock_front_buffer(gs);
    printf("front buffer: %s, stride %u, modifier 0x%llx\n", bo ? "locked" : "FAILED",
           bo ? gbm_bo_get_stride(bo) : 0, bo ? (unsigned long long)gbm_bo_get_modifier(bo) : 0ULL);
    return 0;
}

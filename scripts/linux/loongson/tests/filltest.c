/* Desktop-compositing-like fill rate: full-screen 1920x1080 quads that sample
 * a 1920x1080 RGBA texture and blend over the target (what a compositor does
 * per window), plus a plain opaque copy. Prints Mpixel/s for each. */
#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define W 1920
#define H 1080

static double now(void)
{
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return t.tv_sec + t.tv_nsec * 1e-9;
}

static void run(const char *what, int blend, int draws)
{
    unsigned char p[4];
    if (blend) glEnable(GL_BLEND); else glDisable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    for (int i = 0; i < 3; i++) glDrawArrays(GL_TRIANGLES, 0, 3);
    glReadPixels(W / 2, H / 2, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, p);
    double t0 = now();
    for (int i = 0; i < draws; i++) glDrawArrays(GL_TRIANGLES, 0, 3);
    glReadPixels(W / 2, H / 2, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, p);
    double s = now() - t0;
    printf("  %-22s %6.1f ms per 1080p layer, %6.0f Mpixel/s  (pixel %d %d %d)\n", what,
           s * 1000.0 / draws, (double)W * H * draws / s / 1e6, p[0], p[1], p[2]);
}

int main(void)
{
    EGLDisplay d = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    eglInitialize(d, NULL, NULL);
    const EGLint ca[] = {EGL_SURFACE_TYPE, EGL_PBUFFER_BIT, EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
                         EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8, EGL_NONE};
    EGLConfig c;
    EGLint n = 0;
    eglChooseConfig(d, ca, &c, 1, &n);
    const EGLint sa[] = {EGL_WIDTH, W, EGL_HEIGHT, H, EGL_NONE};
    const EGLint xa[] = {EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE};
    EGLSurface s = eglCreatePbufferSurface(d, c, sa);
    EGLContext x = eglCreateContext(d, c, EGL_NO_CONTEXT, xa);
    eglMakeCurrent(d, s, s, x);
    printf("%s\n", glGetString(GL_RENDERER));

    unsigned char *img = malloc(W * H * 4);
    for (int i = 0; i < W * H; i++) {
        img[i * 4 + 0] = i & 255; img[i * 4 + 1] = (i >> 8) & 255;
        img[i * 4 + 2] = (i >> 4) & 255; img[i * 4 + 3] = 200;
    }
    GLuint tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, W, H, 0, GL_RGBA, GL_UNSIGNED_BYTE, img);

    const char *vs = "attribute vec2 p; varying vec2 uv; void main() { uv = p * 0.5 + 0.5; gl_Position = vec4(p, 0.0, 1.0); }";
    const char *fs = "precision mediump float; varying vec2 uv; uniform sampler2D t; void main() { gl_FragColor = texture2D(t, uv); }";
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
    glViewport(0, 0, W, H);

    run("textured copy", 0, 40);
    run("textured alpha blend", 1, 40);
    return 0;
}

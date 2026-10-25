/* Can the GC1000 draw into the LS7A1000's VRAM? Makes a 1920x1080 dumb buffer
 * on the display card (VRAM), shares it with the GPU through a dma-buf, has the
 * GPU clear it and copy a texture into it, and reads VRAM back from the CPU.
 * Also times a frame (one full-screen draw) into VRAM and into system memory.
 * Usage: EGL_PLATFORM=surfaceless vramtest [/dev/dri/card0] */
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <drm_fourcc.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <time.h>
#include <xf86drm.h>

#define W 1920
#define H 1080
#ifndef EGL_IMAGE_EXTERNAL_FLUSH_EXT
#define EGL_IMAGE_EXTERNAL_FLUSH_EXT 0x32A2 /* EGL_EXT_image_flush_external */
#endif

static double now(void)
{
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return t.tv_sec + t.tv_nsec * 1e-9;
}

static uint32_t *vram;
static uint32_t pitch;

static void check(const char *what, uint32_t want)
{
    uint32_t got = vram[(H / 2) * (pitch / 4) + W / 2] & 0xffffff;
    uint32_t corner = vram[0] & 0xffffff;
    printf("  %-28s VRAM centre 0x%06x corner 0x%06x  (want 0x%06x) %s\n", what, got, corner,
           want & 0xffffff, got == (want & 0xffffff) ? "OK" : "WRONG");
}

static double frames(GLuint fb, int n)
{
    glBindFramebuffer(GL_FRAMEBUFFER, fb);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glFinish();
    double t0 = now();
    for (int i = 0; i < n; i++) {
        glDrawArrays(GL_TRIANGLES, 0, 3);
        glFinish();
    }
    return (now() - t0) * 1000.0 / n;
}

int main(int argc, char **argv)
{
    const char *card = argc > 1 ? argv[1] : "/dev/dri/card0";
    int kfd = open(card, O_RDWR | O_CLOEXEC);
    if (kfd < 0) { perror(card); return 1; }
    drmVersionPtr v = drmGetVersion(kfd);
    printf("display card %s: %s\n", card, v ? v->name : "?");

    struct drm_mode_create_dumb cd = {.width = W, .height = H, .bpp = 32};
    if (drmIoctl(kfd, DRM_IOCTL_MODE_CREATE_DUMB, &cd)) { perror("CREATE_DUMB"); return 1; }
    pitch = cd.pitch;
    struct drm_mode_map_dumb md = {.handle = cd.handle};
    if (drmIoctl(kfd, DRM_IOCTL_MODE_MAP_DUMB, &md)) { perror("MAP_DUMB"); return 1; }
    vram = mmap(NULL, cd.size, PROT_READ | PROT_WRITE, MAP_SHARED, kfd, md.offset);
    if (vram == MAP_FAILED) { perror("mmap"); return 1; }
    for (uint32_t i = 0; i < cd.size / 4; i++) vram[i] = 0x123456;
    int dfd = -1;
    if (drmPrimeHandleToFD(kfd, cd.handle, DRM_CLOEXEC | DRM_RDWR, &dfd)) { perror("export"); return 1; }
    printf("dumb buffer %ux%u pitch %u size %llu exported as fd %d\n", W, H, pitch,
           (unsigned long long)cd.size, dfd);

    EGLDisplay d = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    eglInitialize(d, NULL, NULL);
    const char *ext = eglQueryString(d, EGL_EXTENSIONS);
    int flushExternal = ext && strstr(ext, "EGL_EXT_image_flush_external") != NULL;
    const EGLint ca[] = {EGL_SURFACE_TYPE, EGL_PBUFFER_BIT, EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
                         EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8, EGL_NONE};
    EGLConfig c;
    EGLint n = 0;
    eglChooseConfig(d, ca, &c, 1, &n);
    const EGLint sa[] = {EGL_WIDTH, 16, EGL_HEIGHT, 16, EGL_NONE};
    const EGLint xa[] = {EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE};
    EGLSurface s = eglCreatePbufferSurface(d, c, sa);
    EGLContext x = eglCreateContext(d, c, EGL_NO_CONTEXT, xa);
    eglMakeCurrent(d, s, s, x);
    printf("GPU: %s, EGL_EXT_image_flush_external: %s\n", glGetString(GL_RENDERER),
           flushExternal ? "yes" : "no");

    EGLint ia[32], k = 0;
    ia[k++] = EGL_WIDTH; ia[k++] = W;
    ia[k++] = EGL_HEIGHT; ia[k++] = H;
    ia[k++] = EGL_LINUX_DRM_FOURCC_EXT; ia[k++] = DRM_FORMAT_XRGB8888;
    ia[k++] = EGL_DMA_BUF_PLANE0_FD_EXT; ia[k++] = dfd;
    ia[k++] = EGL_DMA_BUF_PLANE0_OFFSET_EXT; ia[k++] = 0;
    ia[k++] = EGL_DMA_BUF_PLANE0_PITCH_EXT; ia[k++] = (EGLint)pitch;
    if (flushExternal) { ia[k++] = EGL_IMAGE_EXTERNAL_FLUSH_EXT; ia[k++] = EGL_TRUE; }
    ia[k++] = EGL_NONE;
    PFNEGLCREATEIMAGEKHRPROC createImage = (PFNEGLCREATEIMAGEKHRPROC)eglGetProcAddress("eglCreateImageKHR");
    PFNGLEGLIMAGETARGETTEXTURE2DOESPROC targetTexture =
        (PFNGLEGLIMAGETARGETTEXTURE2DOESPROC)eglGetProcAddress("glEGLImageTargetTexture2DOES");
    EGLImageKHR img = createImage(d, EGL_NO_CONTEXT, EGL_LINUX_DMA_BUF_EXT, NULL, ia);
    if (img == EGL_NO_IMAGE_KHR) { printf("eglCreateImage failed 0x%x\n", eglGetError()); return 1; }

    GLuint vtex, vfb, stex, sfb;
    glGenTextures(1, &vtex);
    glBindTexture(GL_TEXTURE_2D, vtex);
    targetTexture(GL_TEXTURE_2D, img);
    glGenFramebuffers(1, &vfb);
    glBindFramebuffer(GL_FRAMEBUFFER, vfb);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, vtex, 0);
    printf("VRAM framebuffer: 0x%x (complete 0x%x), GL error 0x%x\n",
           glCheckFramebufferStatus(GL_FRAMEBUFFER), GL_FRAMEBUFFER_COMPLETE, glGetError());

    check("before drawing", 0x123456);
    glViewport(0, 0, W, H);
    glClearColor(1, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    glFinish();
    check("after GPU clear to red", 0xff0000);

    /* A texture in system memory with a known centre, copied over the screen. */
    uint32_t *pix = malloc(W * H * 4);
    for (int i = 0; i < W * H; i++) pix[i] = 0xff20c040; /* RGBA bytes 40 c0 20 ff */
    glGenTextures(1, &stex);
    glBindTexture(GL_TEXTURE_2D, stex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, W, H, 0, GL_RGBA, GL_UNSIGNED_BYTE, pix);
    const char *vs = "attribute vec2 p; varying vec2 uv; void main() { uv = p * 0.5 + 0.5; gl_Position = vec4(p, 0.0, 1.0); }";
    const char *fs = "precision mediump float; varying vec2 uv; uniform sampler2D t; void main() { gl_FragColor = texture2D(t, uv); }";
    GLuint vsh = glCreateShader(GL_VERTEX_SHADER), fsh = glCreateShader(GL_FRAGMENT_SHADER), prog = glCreateProgram();
    glShaderSource(vsh, 1, &vs, NULL); glCompileShader(vsh);
    glShaderSource(fsh, 1, &fs, NULL); glCompileShader(fsh);
    glAttachShader(prog, vsh); glAttachShader(prog, fsh);
    glBindAttribLocation(prog, 0, "p");
    glLinkProgram(prog);
    glUseProgram(prog);
    const float tri[] = {-1, -1, 3, -1, -1, 3};
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, tri);
    glEnableVertexAttribArray(0);

    double vms = frames(vfb, 30);
    check("after GPU texture copy", 0x40c020);

    /* The same frames into a texture of the GPU's own (system memory). */
    GLuint otex;
    glGenTextures(1, &otex);
    glBindTexture(GL_TEXTURE_2D, otex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, W, H, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glGenFramebuffers(1, &sfb);
    glBindFramebuffer(GL_FRAMEBUFFER, sfb);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, otex, 0);
    glBindTexture(GL_TEXTURE_2D, stex);
    double sms = frames(sfb, 30);

    printf("one full-screen 1080p textured frame: into VRAM %.1f ms, into system memory %.1f ms\n", vms, sms);
    fflush(stdout);
    /* HOLD=<s> keeps the buffer alive to look at the display driver's debugfs */
    if (getenv("HOLD")) {
        struct timespec t = {atoi(getenv("HOLD")), 0};
        nanosleep(&t, NULL);
    }
    return 0;
}

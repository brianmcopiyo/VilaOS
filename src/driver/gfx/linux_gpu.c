#include "linux_gpu.h"
#include "linux_drm_priv.h"
#include "linux_gfx.h"
#include "fb.h"
#include "util.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdint.h>

#if defined(VILAOS_HAVE_LINUX_GFX) && !defined(PHONE_DESKTOP) && !defined(PHONE_VFB) && !defined(_WIN32)

#include <fcntl.h>
#include <unistd.h>
#include <dlfcn.h>
#include <dirent.h>
#include <sys/mman.h>
#include <xf86drm.h>
#include <xf86drmMode.h>
#include <drm_fourcc.h>

#ifndef GBM_BO_USE_SCANOUT
#define GBM_BO_USE_SCANOUT (1 << 0)
#endif
#ifndef GBM_BO_USE_RENDERING
#define GBM_BO_USE_RENDERING (1 << 2)
#endif
#ifndef GBM_BO_USE_LINEAR
#define GBM_BO_USE_LINEAR (1 << 4)
#endif
#ifndef GBM_FORMAT_XRGB8888
#define GBM_FORMAT_XRGB8888 0x34325258
#endif

typedef struct gbm_device gbm_device;
typedef struct gbm_bo gbm_bo;
typedef struct gbm_surface gbm_surface;

typedef union {
    void *ptr;
    int32_t s32;
    uint32_t u32;
    uint64_t u64;
} GbmBoHandle;

typedef void *EGLDisplay;
typedef void *EGLConfig;
typedef void *EGLContext;
typedef void *EGLSurface;
typedef void *EGLNativeDisplayType;
typedef unsigned int EGLenum;
typedef int EGLint;
typedef unsigned int EGLBoolean;
typedef void (*PFNGLCLEARfn)(unsigned int);
typedef void (*PFNGLCLEARCOLORfn)(float, float, float, float);
typedef void (*PFNGLVIEWPORTfn)(int, int, int, int);
typedef void (*PFNGLGENTEXTURESfn)(int, unsigned int *);
typedef void (*PFNGLBINDTEXTUREfn)(unsigned int, unsigned int);
typedef void (*PFNGLTEXIMAGE2Dfn)(unsigned int, int, int, int, int, int, unsigned int, unsigned int, const void *);
typedef void (*PFNGLTEXPARAMETERIfn)(unsigned int, unsigned int, int);
typedef void (*PFNGLGENFRAMEBUFFERSfn)(int, unsigned int *);
typedef void (*PFNGLBINDFRAMEBUFFERfn)(unsigned int, unsigned int);
typedef void (*PFNGLFRAMEBUFFERTEXTUREfn)(unsigned int, unsigned int, unsigned int, unsigned int, int);
typedef void (*PFNGLBLITfn)(int, int, int, int, int, int, int, int, unsigned int, unsigned int);

#define EGL_DEFAULT_DISPLAY ((EGLNativeDisplayType)0)
#define EGL_NO_DISPLAY ((EGLDisplay)0)
#define EGL_NO_CONTEXT ((EGLContext)0)
#define EGL_NO_SURFACE ((EGLSurface)0)
#define EGL_SUCCESS 0x3000
#define EGL_OPENGL_ES_API 0x30A0
#define EGL_SURFACE_TYPE 0x3033
#define EGL_WINDOW_BIT 0x0004
#define EGL_RENDERABLE_TYPE 0x3040
#define EGL_OPENGL_ES2_BIT 0x0004
#define EGL_RED_SIZE 0x3024
#define EGL_GREEN_SIZE 0x3023
#define EGL_BLUE_SIZE 0x3022
#define EGL_ALPHA_SIZE 0x3021
#define EGL_NONE 0x3038
#define EGL_CONTEXT_CLIENT_VERSION 0x3098
#define EGL_PLATFORM_GBM_KHR 0x31D7
#define GL_COLOR_BUFFER_BIT 0x00004000
#define GL_TEXTURE_2D 0x0DE1
#define GL_RGBA 0x1908
#define GL_UNSIGNED_BYTE 0x1401
#define GL_TEXTURE_MIN_FILTER 0x2801
#define GL_TEXTURE_MAG_FILTER 0x2800
#define GL_NEAREST 0x2600
#define GL_FRAMEBUFFER 0x8D40
#define GL_COLOR_ATTACHMENT0 0x8CE0
#define GL_READ_FRAMEBUFFER 0x8CA8
#define GL_DRAW_FRAMEBUFFER 0x8CA9

typedef struct {
    void *lib_gbm;
    void *lib_egl;
    void *lib_gles;
    gbm_device *(*gbm_create_device)(int);
    void (*gbm_device_destroy)(gbm_device *);
    gbm_bo *(*gbm_bo_create)(gbm_device *, uint32_t, uint32_t, uint32_t, uint32_t);
    void (*gbm_bo_destroy)(gbm_bo *);
    GbmBoHandle (*gbm_bo_get_handle)(gbm_bo *);
    uint32_t (*gbm_bo_get_stride)(gbm_bo *);
    void *(*gbm_bo_map)(gbm_bo *, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t *, void **);
    void (*gbm_bo_unmap)(gbm_bo *, void *);
    gbm_surface *(*gbm_surface_create)(gbm_device *, uint32_t, uint32_t, uint32_t, uint32_t);
    void (*gbm_surface_destroy)(gbm_surface *);
    gbm_bo *(*gbm_surface_lock_front_buffer)(gbm_surface *);
    void (*gbm_surface_release_buffer)(gbm_surface *, gbm_bo *);
    int (*gbm_surface_has_free_buffers)(gbm_surface *);
    EGLDisplay (*eglGetDisplay)(EGLNativeDisplayType);
    EGLDisplay (*eglGetPlatformDisplay)(EGLenum, void *, const EGLint *);
    EGLBoolean (*eglInitialize)(EGLDisplay, EGLint *, EGLint *);
    EGLBoolean (*eglTerminate)(EGLDisplay);
    EGLBoolean (*eglBindAPI)(EGLenum);
    EGLBoolean (*eglChooseConfig)(EGLDisplay, const EGLint *, EGLConfig *, EGLint, EGLint *);
    EGLContext (*eglCreateContext)(EGLDisplay, EGLConfig, EGLContext, const EGLint *);
    EGLBoolean (*eglDestroyContext)(EGLDisplay, EGLContext);
    EGLSurface (*eglCreateWindowSurface)(EGLDisplay, EGLConfig, void *, const EGLint *);
    EGLBoolean (*eglDestroySurface)(EGLDisplay, EGLSurface);
    EGLBoolean (*eglMakeCurrent)(EGLDisplay, EGLSurface, EGLSurface, EGLContext);
    EGLBoolean (*eglSwapBuffers)(EGLDisplay, EGLSurface);
    EGLint (*eglGetError)(void);
    void *(*eglGetProcAddress)(const char *);
    PFNGLCLEARfn glClear;
    PFNGLCLEARCOLORfn glClearColor;
    PFNGLVIEWPORTfn glViewport;
    PFNGLGENTEXTURESfn glGenTextures;
    PFNGLBINDTEXTUREfn glBindTexture;
    PFNGLTEXIMAGE2Dfn glTexImage2D;
    PFNGLTEXPARAMETERIfn glTexParameteri;
    int have_egl;
} GpuApi;

typedef struct {
    int fd;
    int own_fd;
    uint32_t conn_id;
    uint32_t crtc_id;
    drmModeModeInfo mode;
    int w;
    int h;
    gbm_device *dev;
    gbm_bo *bo[2];
    uint32_t fb_id[2];
    int front;
    EGLDisplay dpy;
    EGLConfig cfg;
    EGLContext ctx;
    EGLSurface surf;
    gbm_surface *gsurf;
    int egl_ok;
    char driver[64];
    long vram_mb;
    GpuApi api;
} LinuxGpuState;

static LinuxGpuState g_gpu;

static void *dlsym_req(void *h, const char *name)
{
    return h ? dlsym(h, name) : NULL;
}

static long read_vram_mb(int card_index)
{
    char path[128];
    snprintf(path, sizeof(path), "/sys/class/drm/card%d/device/mem_info_vram_total", card_index);
    FILE *f = fopen(path, "r");
    if (!f) {
        snprintf(path, sizeof(path), "/sys/class/drm/card%d/device/mem_info_vis_vram_total", card_index);
        f = fopen(path, "r");
    }
    if (!f)
        return 0;
    unsigned long long bytes = 0;
    if (fscanf(f, "%llu", &bytes) != 1) {
        fclose(f);
        return 0;
    }
    fclose(f);
    return (long)(bytes / (1024ull * 1024ull));
}

static void read_driver_name(int card_index, char *out, size_t n)
{
    char path[128];
    snprintf(path, sizeof(path), "/sys/class/drm/card%d/device/driver", card_index);
    char link[256];
    ssize_t len = readlink(path, link, sizeof(link) - 1);
    if (len > 0) {
        link[len] = 0;
        const char *base = strrchr(link, '/');
        str_copy(out, n, base ? base + 1 : link);
        return;
    }
    snprintf(path, sizeof(path), "/sys/class/drm/card%d/device/uevent", card_index);
    FILE *f = fopen(path, "r");
    if (!f) {
        str_copy(out, n, "drm");
        return;
    }
    char line[256];
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "DRIVER=", 7) == 0) {
            char *v = line + 7;
            size_t L = strlen(v);
            while (L > 0 && (v[L - 1] == '\n' || v[L - 1] == '\r'))
                v[--L] = 0;
            str_copy(out, n, v);
            fclose(f);
            return;
        }
    }
    fclose(f);
    str_copy(out, n, "drm");
}

static int pick_connector(int fd, drmModeRes *res, drmModeConnector **out_conn, drmModeModeInfo *out_mode,
                          uint32_t *out_crtc)
{
    for (int i = 0; i < res->count_connectors; i++) {
        drmModeConnector *conn = drmModeGetConnector(fd, res->connectors[i]);
        if (!conn)
            continue;
        if (conn->connection == DRM_MODE_CONNECTED && conn->count_modes > 0) {
            drmModeEncoder *enc = NULL;
            uint32_t crtc = 0;
            if (conn->encoder_id)
                enc = drmModeGetEncoder(fd, conn->encoder_id);
            if (enc) {
                crtc = enc->crtc_id;
                drmModeFreeEncoder(enc);
            }
            if (!crtc && res->count_crtcs > 0)
                crtc = res->crtcs[0];
            if (crtc) {
                int best = 0;
                for (int m = 0; m < conn->count_modes; m++) {
                    if (conn->modes[m].type & DRM_MODE_TYPE_PREFERRED) {
                        best = m;
                        break;
                    }
                }
                *out_conn = conn;
                *out_mode = conn->modes[best];
                *out_crtc = crtc;
                return 0;
            }
        }
        drmModeFreeConnector(conn);
    }
    return -1;
}

static int load_gbm(GpuApi *a)
{
    static const char *names[] = {"libgbm.so.1", "libgbm.so", NULL};
    for (int i = 0; names[i]; i++) {
        a->lib_gbm = dlopen(names[i], RTLD_NOW | RTLD_LOCAL);
        if (a->lib_gbm)
            break;
    }
    if (!a->lib_gbm)
        return -1;
    a->gbm_create_device = dlsym_req(a->lib_gbm, "gbm_create_device");
    a->gbm_device_destroy = dlsym_req(a->lib_gbm, "gbm_device_destroy");
    a->gbm_bo_create = dlsym_req(a->lib_gbm, "gbm_bo_create");
    a->gbm_bo_destroy = dlsym_req(a->lib_gbm, "gbm_bo_destroy");
    a->gbm_bo_get_stride = dlsym_req(a->lib_gbm, "gbm_bo_get_stride");
    a->gbm_bo_map = dlsym_req(a->lib_gbm, "gbm_bo_map");
    a->gbm_bo_unmap = dlsym_req(a->lib_gbm, "gbm_bo_unmap");
    a->gbm_bo_get_handle = (GbmBoHandle (*)(gbm_bo *))dlsym_req(a->lib_gbm, "gbm_bo_get_handle");
    a->gbm_surface_create = dlsym_req(a->lib_gbm, "gbm_surface_create");
    a->gbm_surface_destroy = dlsym_req(a->lib_gbm, "gbm_surface_destroy");
    a->gbm_surface_lock_front_buffer = dlsym_req(a->lib_gbm, "gbm_surface_lock_front_buffer");
    a->gbm_surface_release_buffer = dlsym_req(a->lib_gbm, "gbm_surface_release_buffer");
    if (!a->gbm_create_device || !a->gbm_device_destroy || !a->gbm_bo_create || !a->gbm_bo_destroy ||
        !a->gbm_bo_get_stride || !a->gbm_bo_get_handle)
        return -1;
    return 0;
}

static int load_egl(GpuApi *a)
{
    static const char *egl_names[] = {"libEGL.so.1", "libEGL.so", NULL};
    static const char *gles_names[] = {"libGLESv2.so.2", "libGLESv2.so", NULL};
    for (int i = 0; egl_names[i]; i++) {
        a->lib_egl = dlopen(egl_names[i], RTLD_NOW | RTLD_LOCAL);
        if (a->lib_egl)
            break;
    }
    if (!a->lib_egl)
        return -1;
    for (int i = 0; gles_names[i]; i++) {
        a->lib_gles = dlopen(gles_names[i], RTLD_NOW | RTLD_LOCAL);
        if (a->lib_gles)
            break;
    }
    a->eglGetDisplay = dlsym_req(a->lib_egl, "eglGetDisplay");
    a->eglGetPlatformDisplay = dlsym_req(a->lib_egl, "eglGetPlatformDisplay");
    if (!a->eglGetPlatformDisplay)
        a->eglGetPlatformDisplay = dlsym_req(a->lib_egl, "eglGetPlatformDisplayEXT");
    a->eglInitialize = dlsym_req(a->lib_egl, "eglInitialize");
    a->eglTerminate = dlsym_req(a->lib_egl, "eglTerminate");
    a->eglBindAPI = dlsym_req(a->lib_egl, "eglBindAPI");
    a->eglChooseConfig = dlsym_req(a->lib_egl, "eglChooseConfig");
    a->eglCreateContext = dlsym_req(a->lib_egl, "eglCreateContext");
    a->eglDestroyContext = dlsym_req(a->lib_egl, "eglDestroyContext");
    a->eglCreateWindowSurface = dlsym_req(a->lib_egl, "eglCreateWindowSurface");
    a->eglDestroySurface = dlsym_req(a->lib_egl, "eglDestroySurface");
    a->eglMakeCurrent = dlsym_req(a->lib_egl, "eglMakeCurrent");
    a->eglSwapBuffers = dlsym_req(a->lib_egl, "eglSwapBuffers");
    a->eglGetError = dlsym_req(a->lib_egl, "eglGetError");
    a->eglGetProcAddress = dlsym_req(a->lib_egl, "eglGetProcAddress");
    if (!a->eglInitialize || !a->eglBindAPI || !a->eglChooseConfig || !a->eglCreateContext ||
        !a->eglMakeCurrent || !a->eglSwapBuffers)
        return -1;
    if (a->lib_gles) {
        a->glClear = (PFNGLCLEARfn)dlsym_req(a->lib_gles, "glClear");
        a->glClearColor = (PFNGLCLEARCOLORfn)dlsym_req(a->lib_gles, "glClearColor");
        a->glViewport = (PFNGLVIEWPORTfn)dlsym_req(a->lib_gles, "glViewport");
        a->glGenTextures = (PFNGLGENTEXTURESfn)dlsym_req(a->lib_gles, "glGenTextures");
        a->glBindTexture = (PFNGLBINDTEXTUREfn)dlsym_req(a->lib_gles, "glBindTexture");
        a->glTexImage2D = (PFNGLTEXIMAGE2Dfn)dlsym_req(a->lib_gles, "glTexImage2D");
        a->glTexParameteri = (PFNGLTEXPARAMETERIfn)dlsym_req(a->lib_gles, "glTexParameteri");
    }
    a->have_egl = 1;
    return 0;
}

static void unload_api(GpuApi *a)
{
    if (a->lib_gles)
        dlclose(a->lib_gles);
    if (a->lib_egl)
        dlclose(a->lib_egl);
    if (a->lib_gbm)
        dlclose(a->lib_gbm);
    memset(a, 0, sizeof(*a));
}

static int create_bos(LinuxGpuState *s, int w, int h)
{
    GpuApi *a = &s->api;
    uint32_t flags = GBM_BO_USE_SCANOUT | GBM_BO_USE_RENDERING | GBM_BO_USE_LINEAR;
    for (int i = 0; i < 2; i++) {
        s->bo[i] = a->gbm_bo_create(s->dev, (uint32_t)w, (uint32_t)h, GBM_FORMAT_XRGB8888, flags);
        if (!s->bo[i])
            s->bo[i] = a->gbm_bo_create(s->dev, (uint32_t)w, (uint32_t)h, GBM_FORMAT_XRGB8888,
                                        GBM_BO_USE_SCANOUT | GBM_BO_USE_LINEAR);
        if (!s->bo[i])
            return -1;
        GbmBoHandle hh = a->gbm_bo_get_handle(s->bo[i]);
        uint32_t hnd = hh.u32;
        uint32_t stride = a->gbm_bo_get_stride(s->bo[i]);
        if (drmModeAddFB(s->fd, (uint32_t)w, (uint32_t)h, 24, 32, stride, hnd, &s->fb_id[i]) != 0)
            return -1;
    }
    s->front = 0;
    return 0;
}

static void destroy_bos(LinuxGpuState *s)
{
    GpuApi *a = &s->api;
    for (int i = 0; i < 2; i++) {
        if (s->fb_id[i] && s->fd >= 0)
            drmModeRmFB(s->fd, s->fb_id[i]);
        s->fb_id[i] = 0;
        if (s->bo[i] && a->gbm_bo_destroy)
            a->gbm_bo_destroy(s->bo[i]);
        s->bo[i] = NULL;
    }
}

static int try_egl(LinuxGpuState *s, int w, int h)
{
    GpuApi *a = &s->api;
    if (load_egl(a) != 0)
        return -1;
    if (!a->gbm_surface_create)
        return -1;
    s->gsurf = a->gbm_surface_create(s->dev, (uint32_t)w, (uint32_t)h, GBM_FORMAT_XRGB8888,
                                     GBM_BO_USE_SCANOUT | GBM_BO_USE_RENDERING);
    if (!s->gsurf)
        return -1;
    if (a->eglGetPlatformDisplay)
        s->dpy = a->eglGetPlatformDisplay(EGL_PLATFORM_GBM_KHR, s->dev, NULL);
    else if (a->eglGetDisplay)
        s->dpy = a->eglGetDisplay((EGLNativeDisplayType)s->dev);
    if (!s->dpy || s->dpy == EGL_NO_DISPLAY)
        return -1;
    if (!a->eglInitialize(s->dpy, NULL, NULL))
        return -1;
    if (!a->eglBindAPI(EGL_OPENGL_ES_API))
        return -1;
    EGLint attrs[] = {
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
        EGL_RED_SIZE, 8,
        EGL_GREEN_SIZE, 8,
        EGL_BLUE_SIZE, 8,
        EGL_NONE
    };
    EGLint n = 0;
    if (!a->eglChooseConfig(s->dpy, attrs, &s->cfg, 1, &n) || n < 1)
        return -1;
    EGLint ctx_attrs[] = {EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE};
    s->ctx = a->eglCreateContext(s->dpy, s->cfg, EGL_NO_CONTEXT, ctx_attrs);
    if (!s->ctx || s->ctx == EGL_NO_CONTEXT)
        return -1;
    s->surf = a->eglCreateWindowSurface(s->dpy, s->cfg, (void *)s->gsurf, NULL);
    if (!s->surf || s->surf == EGL_NO_SURFACE)
        return -1;
    if (!a->eglMakeCurrent(s->dpy, s->surf, s->surf, s->ctx))
        return -1;
    if (a->glViewport)
        a->glViewport(0, 0, w, h);
    s->egl_ok = 1;
    return 0;
}

static void teardown_egl(LinuxGpuState *s)
{
    GpuApi *a = &s->api;
    if (s->dpy && s->dpy != EGL_NO_DISPLAY && a->eglMakeCurrent)
        a->eglMakeCurrent(s->dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    if (s->surf && a->eglDestroySurface)
        a->eglDestroySurface(s->dpy, s->surf);
    if (s->ctx && a->eglDestroyContext)
        a->eglDestroyContext(s->dpy, s->ctx);
    if (s->dpy && a->eglTerminate)
        a->eglTerminate(s->dpy);
    if (s->gsurf && a->gbm_surface_destroy)
        a->gbm_surface_destroy(s->gsurf);
    s->surf = EGL_NO_SURFACE;
    s->ctx = EGL_NO_CONTEXT;
    s->dpy = EGL_NO_DISPLAY;
    s->gsurf = NULL;
    s->egl_ok = 0;
}

int linux_gpu_probe(char *name, size_t name_n, long *vram_mb)
{
    if (name && name_n)
        name[0] = 0;
    if (vram_mb)
        *vram_mb = 0;
    char path[64];
    for (int i = 0; i < 16; i++) {
        snprintf(path, sizeof(path), "/dev/dri/card%d", i);
        if (access(path, R_OK | W_OK) != 0)
            continue;
        if (name && name_n)
            read_driver_name(i, name, name_n);
        if (vram_mb)
            *vram_mb = read_vram_mb(i);
        snprintf(path, sizeof(path), "/dev/dri/renderD%d", 128 + i);
        return 0;
    }
    DIR *d = opendir("/dev/dri");
    if (!d)
        return -1;
    struct dirent *ent;
    int found = 0;
    while ((ent = readdir(d)) != NULL) {
        if (strncmp(ent->d_name, "card", 4) != 0)
            continue;
        found = 1;
        int idx = atoi(ent->d_name + 4);
        if (name && name_n)
            read_driver_name(idx, name, name_n);
        if (vram_mb)
            *vram_mb = read_vram_mb(idx);
        break;
    }
    closedir(d);
    return found ? 0 : -1;
}

int linux_gpu_open(Gfx *g)
{
    if (!g)
        return -1;
    linux_gpu_close(g);
    memset(&g_gpu, 0, sizeof(g_gpu));
    g_gpu.fd = -1;
    char path[64];
    int card_idx = -1;
    for (int i = 0; i < 16; i++) {
        snprintf(path, sizeof(path), "/dev/dri/card%d", i);
        int fd = open(path, O_RDWR | O_CLOEXEC);
        if (fd < 0)
            continue;
        drmModeRes *res = drmModeGetResources(fd);
        if (!res) {
            close(fd);
            continue;
        }
        drmModeConnector *conn = NULL;
        drmModeModeInfo mode;
        uint32_t crtc = 0;
        memset(&mode, 0, sizeof(mode));
        if (pick_connector(fd, res, &conn, &mode, &crtc) != 0) {
            drmModeFreeResources(res);
            close(fd);
            continue;
        }
        g_gpu.fd = fd;
        g_gpu.own_fd = 1;
        g_gpu.conn_id = conn->connector_id;
        g_gpu.crtc_id = crtc;
        g_gpu.mode = mode;
        g_gpu.w = mode.hdisplay;
        g_gpu.h = mode.vdisplay;
        card_idx = i;
        drmModeFreeConnector(conn);
        drmModeFreeResources(res);
        break;
    }
    if (g_gpu.fd < 0)
        return -1;
    read_driver_name(card_idx, g_gpu.driver, sizeof(g_gpu.driver));
    g_gpu.vram_mb = read_vram_mb(card_idx);
    if (load_gbm(&g_gpu.api) != 0) {
        linux_gpu_close(g);
        return -1;
    }
    g_gpu.dev = g_gpu.api.gbm_create_device(g_gpu.fd);
    if (!g_gpu.dev) {
        linux_gpu_close(g);
        return -1;
    }
    if (try_egl(&g_gpu, g_gpu.w, g_gpu.h) != 0) {
        teardown_egl(&g_gpu);
        if (create_bos(&g_gpu, g_gpu.w, g_gpu.h) != 0) {
            linux_gpu_close(g);
            return -1;
        }
        if (drmModeSetCrtc(g_gpu.fd, g_gpu.crtc_id, g_gpu.fb_id[0], 0, 0, &g_gpu.conn_id, 1, &g_gpu.mode) != 0) {
            linux_gpu_close(g);
            return -1;
        }
    } else {
        if (create_bos(&g_gpu, g_gpu.w, g_gpu.h) != 0) {
            linux_gpu_close(g);
            return -1;
        }
        if (drmModeSetCrtc(g_gpu.fd, g_gpu.crtc_id, g_gpu.fb_id[0], 0, 0, &g_gpu.conn_id, 1, &g_gpu.mode) != 0) {
            linux_gpu_close(g);
            return -1;
        }
    }
    g->dri_fd = g_gpu.fd;
    g->fb_w = g_gpu.w;
    g->fb_h = g_gpu.h;
    g->fb_bpp = 32;
    g->is_xrgb = 1;
    g->gfx_tier = GFX_TIER_GPU;
    g->gpu_accel = g_gpu.egl_ok ? 1 : 1;
    g->vram_mb = g_gpu.vram_mb;
    g->gpu = &g_gpu;
    str_copy(g->backend, sizeof(g->backend), g_gpu.egl_ok ? "gpu-egl" : "gpu-gbm");
    str_copy(g->gpu_name, sizeof(g->gpu_name), g_gpu.driver);
    if (!g->drv_name[0])
        str_copy(g->drv_name, sizeof(g->drv_name), g_gpu.driver);
    if (gfx_set_size(g, g_gpu.w, g_gpu.h) != 0) {
        linux_gpu_close(g);
        return -1;
    }
    return 0;
}

void linux_gpu_close(Gfx *g)
{
    int fd = g_gpu.fd;
    int own = g_gpu.own_fd;
    teardown_egl(&g_gpu);
    destroy_bos(&g_gpu);
    if (g_gpu.dev && g_gpu.api.gbm_device_destroy)
        g_gpu.api.gbm_device_destroy(g_gpu.dev);
    g_gpu.dev = NULL;
    unload_api(&g_gpu.api);
    if (own && fd >= 0)
        close(fd);
    memset(&g_gpu, 0, sizeof(g_gpu));
    g_gpu.fd = -1;
    if (g) {
        g->gpu = NULL;
        if (g->gfx_tier == GFX_TIER_GPU) {
            g->gfx_tier = GFX_TIER_NONE;
            g->gpu_accel = 0;
        }
        if (g->dri_fd == fd)
            g->dri_fd = -1;
    }
}

static int blit_to_bo(LinuxGpuState *s, Gfx *g, int bi)
{
    GpuApi *a = &s->api;
    if (!s->bo[bi] || !a->gbm_bo_map || !a->gbm_bo_unmap)
        return -1;
    uint32_t stride = 0;
    void *map_data = NULL;
    void *map = a->gbm_bo_map(s->bo[bi], 0, 0, (uint32_t)s->w, (uint32_t)s->h, &stride, &map_data);
    if (!map)
        return -1;
    int use_argb = (g->compose_px && g->compose_pixels >= (size_t)g->w * (size_t)g->h);
    for (int y = 0; y < s->h; y++) {
        int sy = (g->h > 0) ? (y * g->h / s->h) : y;
        if (sy < 0)
            sy = 0;
        if (sy >= g->h)
            sy = g->h - 1;
        uint32_t *dst = (uint32_t *)((uint8_t *)map + y * stride);
        for (int x = 0; x < s->w; x++) {
            int sx = (g->w > 0) ? (x * g->w / s->w) : x;
            if (sx < 0)
                sx = 0;
            if (sx >= g->w)
                sx = g->w - 1;
            if (use_argb) {
                dst[x] = g->compose_px[sy * g->w + sx] | 0xFF000000u;
            } else if (g->back) {
                uint16_t c = g->back[sy * g->w + sx];
                uint32_t r = ((c >> 11) & 0x1F) << 3;
                uint32_t gg = ((c >> 5) & 0x3F) << 2;
                uint32_t b = (c & 0x1F) << 3;
                dst[x] = 0xFF000000u | (r << 16) | (gg << 8) | b;
            } else {
                dst[x] = 0xFF000000u;
            }
        }
    }
    a->gbm_bo_unmap(s->bo[bi], map_data);
    return 0;
}

int linux_gpu_present(Gfx *g)
{
    if (!g || g_gpu.fd < 0)
        return -1;
    int next = 1 - g_gpu.front;
    if (g_gpu.egl_ok && g_gpu.api.eglSwapBuffers && g_gpu.surf) {
        if (g_gpu.api.glClearColor)
            g_gpu.api.glClearColor(0.f, 0.f, 0.f, 1.f);
        if (g_gpu.api.glClear)
            g_gpu.api.glClear(GL_COLOR_BUFFER_BIT);
        if (blit_to_bo(&g_gpu, g, next) != 0)
            return -1;
        g_gpu.api.eglSwapBuffers(g_gpu.dpy, g_gpu.surf);
    } else {
        if (blit_to_bo(&g_gpu, g, next) != 0)
            return -1;
    }
    if (drmModePageFlip(g_gpu.fd, g_gpu.crtc_id, g_gpu.fb_id[next], 0, NULL) != 0) {
        if (drmModeSetCrtc(g_gpu.fd, g_gpu.crtc_id, g_gpu.fb_id[next], 0, 0, &g_gpu.conn_id, 1, &g_gpu.mode) != 0)
            return -1;
    }
    g_gpu.front = next;
    return 0;
}

int linux_gpu_resize(Gfx *g, int w, int h)
{
    if (!g || g_gpu.fd < 0 || w < GFX_MIN_W || h < GFX_MIN_H)
        return -1;
    if (w == g_gpu.w && h == g_gpu.h)
        return 0;
    teardown_egl(&g_gpu);
    destroy_bos(&g_gpu);
    g_gpu.w = w;
    g_gpu.h = h;
    g_gpu.mode.hdisplay = (uint16_t)w;
    g_gpu.mode.vdisplay = (uint16_t)h;
    if (try_egl(&g_gpu, w, h) != 0)
        teardown_egl(&g_gpu);
    if (create_bos(&g_gpu, w, h) != 0)
        return -1;
    if (drmModeSetCrtc(g_gpu.fd, g_gpu.crtc_id, g_gpu.fb_id[0], 0, 0, &g_gpu.conn_id, 1, &g_gpu.mode) != 0)
        return -1;
    g->fb_w = w;
    g->fb_h = h;
    return gfx_set_size(g, w, h);
}

int linux_gpu_poll_hotplug(Gfx *g)
{
    if (!g || g_gpu.fd < 0)
        return 0;
    drmModeConnector *conn = drmModeGetConnector(g_gpu.fd, g_gpu.conn_id);
    if (!conn)
        return 0;
    int changed = 0;
    if (conn->connection == DRM_MODE_CONNECTED && conn->count_modes > 0) {
        int best = 0;
        for (int m = 0; m < conn->count_modes; m++) {
            if (conn->modes[m].type & DRM_MODE_TYPE_PREFERRED) {
                best = m;
                break;
            }
        }
        int nw = conn->modes[best].hdisplay;
        int nh = conn->modes[best].vdisplay;
        if (nw >= GFX_MIN_W && nh >= GFX_MIN_H && (nw != g_gpu.w || nh != g_gpu.h)) {
            g_gpu.mode = conn->modes[best];
            if (linux_gpu_resize(g, nw, nh) == 0)
                changed = 1;
        }
    }
    drmModeFreeConnector(conn);
    return changed;
}

int linux_gpu_active(const Gfx *g)
{
    return (g && g->gpu && g->gfx_tier == GFX_TIER_GPU && g_gpu.fd >= 0) ? 1 : 0;
}

const char *linux_gpu_tier_name(int tier)
{
    switch (tier) {
    case GFX_TIER_GPU:
        return "gpu";
    case GFX_TIER_DRM:
        return "drm";
    case GFX_TIER_FB:
        return "fb";
    default:
        return "none";
    }
}

#else

int linux_gpu_probe(char *name, size_t name_n, long *vram_mb)
{
    if (name && name_n)
        name[0] = 0;
    if (vram_mb)
        *vram_mb = 0;
    return -1;
}
int linux_gpu_open(Gfx *g)
{
    (void)g;
    return -1;
}
void linux_gpu_close(Gfx *g)
{
    (void)g;
}
int linux_gpu_present(Gfx *g)
{
    (void)g;
    return -1;
}
int linux_gpu_resize(Gfx *g, int w, int h)
{
    (void)g;
    (void)w;
    (void)h;
    return -1;
}
int linux_gpu_poll_hotplug(Gfx *g)
{
    (void)g;
    return 0;
}
int linux_gpu_active(const Gfx *g)
{
    (void)g;
    return 0;
}
const char *linux_gpu_tier_name(int tier)
{
    (void)tier;
    return "none";
}

#endif

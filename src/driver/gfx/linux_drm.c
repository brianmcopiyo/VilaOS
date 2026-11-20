#include "linux_gfx.h"
#include "util.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#if defined(VILAOS_HAVE_LINUX_GFX) && !defined(PHONE_DESKTOP) && !defined(PHONE_VFB) && !defined(_WIN32)

#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <xf86drm.h>
#include <xf86drmMode.h>
#include <drm_fourcc.h>

typedef struct {
    int fd;
    uint32_t conn_id;
    uint32_t crtc_id;
    uint32_t fb_id;
    uint32_t handle;
    uint32_t pitch;
    uint64_t size;
    void *map;
    drmModeModeInfo mode;
    int w;
    int h;
} LinuxDrmState;

static LinuxDrmState g_drm;

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
                *out_conn = conn;
                *out_mode = conn->modes[0];
                *out_crtc = crtc;
                return 0;
            }
        }
        drmModeFreeConnector(conn);
    }
    return -1;
}

int linux_drm_open(Gfx *g)
{
    if (!g)
        return -1;
    linux_drm_close(g);
    memset(&g_drm, 0, sizeof(g_drm));
    int fd = open("/dev/dri/card0", O_RDWR | O_CLOEXEC);
    if (fd < 0)
        fd = open("/dev/dri/card1", O_RDWR | O_CLOEXEC);
    if (fd < 0)
        return -1;
    drmModeRes *res = drmModeGetResources(fd);
    if (!res) {
        close(fd);
        return -1;
    }
    drmModeConnector *conn = NULL;
    drmModeModeInfo mode;
    uint32_t crtc = 0;
    memset(&mode, 0, sizeof(mode));
    if (pick_connector(fd, res, &conn, &mode, &crtc) != 0) {
        drmModeFreeResources(res);
        close(fd);
        return -1;
    }
    int w = mode.hdisplay;
    int h = mode.vdisplay;
    struct drm_mode_create_dumb creq;
    memset(&creq, 0, sizeof(creq));
    creq.width = (uint32_t)w;
    creq.height = (uint32_t)h;
    creq.bpp = 32;
    if (drmIoctl(fd, DRM_IOCTL_MODE_CREATE_DUMB, &creq) != 0) {
        drmModeFreeConnector(conn);
        drmModeFreeResources(res);
        close(fd);
        return -1;
    }
    uint32_t fb_id = 0;
    if (drmModeAddFB(fd, (uint32_t)w, (uint32_t)h, 24, 32, creq.pitch, creq.handle, &fb_id) != 0) {
        struct drm_mode_destroy_dumb dreq;
        memset(&dreq, 0, sizeof(dreq));
        dreq.handle = creq.handle;
        drmIoctl(fd, DRM_IOCTL_MODE_DESTROY_DUMB, &dreq);
        drmModeFreeConnector(conn);
        drmModeFreeResources(res);
        close(fd);
        return -1;
    }
    struct drm_mode_map_dumb mreq;
    memset(&mreq, 0, sizeof(mreq));
    mreq.handle = creq.handle;
    if (drmIoctl(fd, DRM_IOCTL_MODE_MAP_DUMB, &mreq) != 0) {
        drmModeRmFB(fd, fb_id);
        struct drm_mode_destroy_dumb dreq;
        memset(&dreq, 0, sizeof(dreq));
        dreq.handle = creq.handle;
        drmIoctl(fd, DRM_IOCTL_MODE_DESTROY_DUMB, &dreq);
        drmModeFreeConnector(conn);
        drmModeFreeResources(res);
        close(fd);
        return -1;
    }
    void *map = mmap(NULL, creq.size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, (off_t)mreq.offset);
    if (map == MAP_FAILED) {
        drmModeRmFB(fd, fb_id);
        struct drm_mode_destroy_dumb dreq;
        memset(&dreq, 0, sizeof(dreq));
        dreq.handle = creq.handle;
        drmIoctl(fd, DRM_IOCTL_MODE_DESTROY_DUMB, &dreq);
        drmModeFreeConnector(conn);
        drmModeFreeResources(res);
        close(fd);
        return -1;
    }
    if (drmModeSetCrtc(fd, crtc, fb_id, 0, 0, &conn->connector_id, 1, &mode) != 0) {
        munmap(map, creq.size);
        drmModeRmFB(fd, fb_id);
        struct drm_mode_destroy_dumb dreq;
        memset(&dreq, 0, sizeof(dreq));
        dreq.handle = creq.handle;
        drmIoctl(fd, DRM_IOCTL_MODE_DESTROY_DUMB, &dreq);
        drmModeFreeConnector(conn);
        drmModeFreeResources(res);
        close(fd);
        return -1;
    }
    g_drm.fd = fd;
    g_drm.conn_id = conn->connector_id;
    g_drm.crtc_id = crtc;
    g_drm.fb_id = fb_id;
    g_drm.handle = creq.handle;
    g_drm.pitch = creq.pitch;
    g_drm.size = creq.size;
    g_drm.map = map;
    g_drm.mode = mode;
    g_drm.w = w;
    g_drm.h = h;
    g->dri_fd = fd;
    g->fb_map = map;
    g->fb_map_len = (size_t)creq.size;
    g->fb_w = w;
    g->fb_h = h;
    g->fb_bpp = 32;
    g->fb_stride = (int)creq.pitch;
    g->is_xrgb = 1;
    str_copy(g->backend, sizeof(g->backend), "drm");
    drmModeFreeConnector(conn);
    drmModeFreeResources(res);
    return 0;
}

void linux_drm_close(Gfx *g)
{
    if (g_drm.map && g_drm.size)
        munmap(g_drm.map, (size_t)g_drm.size);
    if (g_drm.fd >= 0) {
        if (g_drm.fb_id)
            drmModeRmFB(g_drm.fd, g_drm.fb_id);
        if (g_drm.handle) {
            struct drm_mode_destroy_dumb dreq;
            memset(&dreq, 0, sizeof(dreq));
            dreq.handle = g_drm.handle;
            drmIoctl(g_drm.fd, DRM_IOCTL_MODE_DESTROY_DUMB, &dreq);
        }
        close(g_drm.fd);
    }
    memset(&g_drm, 0, sizeof(g_drm));
    g_drm.fd = -1;
    if (g) {
        if (g->dri_fd >= 0)
            g->dri_fd = -1;
        g->fb_map = NULL;
        g->fb_map_len = 0;
    }
}

int linux_drm_present(Gfx *g)
{
    if (!g || !g_drm.map || !g->back || !g->dirty.valid)
        return -1;
    int x0 = g->dirty.x0;
    int y0 = g->dirty.y0;
    int x1 = g->dirty.x1;
    int y1 = g->dirty.y1;
    if (x0 < 0)
        x0 = 0;
    if (y0 < 0)
        y0 = 0;
    if (x1 > g->w)
        x1 = g->w;
    if (y1 > g->h)
        y1 = g->h;
    int dst_w = g_drm.w;
    int dst_h = g_drm.h;
    for (int y = y0; y < y1; y++) {
        int dy = y * dst_h / g->h;
        if (dy < 0 || dy >= dst_h)
            continue;
        uint32_t *dst = (uint32_t *)((uint8_t *)g_drm.map + dy * g_drm.pitch);
        for (int x = x0; x < x1; x++) {
            int dx = x * dst_w / g->w;
            if (dx < 0 || dx >= dst_w)
                continue;
            uint16_t c = g->back[y * g->w + x];
            uint32_t r = ((c >> 11) & 0x1F) << 3;
            uint32_t gg = ((c >> 5) & 0x3F) << 2;
            uint32_t b = (c & 0x1F) << 3;
            dst[dx] = 0xFF000000u | (r << 16) | (gg << 8) | b;
        }
    }
    drmModePageFlip(g_drm.fd, g_drm.crtc_id, g_drm.fb_id, 0, NULL);
    return 0;
}

#else

int linux_drm_open(Gfx *g)
{
    (void)g;
    return -1;
}
void linux_drm_close(Gfx *g)
{
    (void)g;
}
int linux_drm_present(Gfx *g)
{
    (void)g;
    return -1;
}

#endif

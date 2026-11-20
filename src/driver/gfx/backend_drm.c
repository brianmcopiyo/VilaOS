#include "gfx_backend.h"
#include "fb.h"
#include "linux_gfx.h"
#include "util.h"

#include <stdio.h>
#include <string.h>

#if !defined(PHONE_DESKTOP) && !defined(PHONE_VFB) && !defined(_WIN32)
#include <fcntl.h>
#include <unistd.h>

int gfx_backend_probe_drm(Gfx *g)
{
    if (!g)
        return -1;
#if defined(VILAOS_HAVE_LINUX_GFX)
    if (linux_drm_open(g) == 0) {
        if (g->w <= 0 || g->h <= 0) {
            if (gfx_set_size(g, PHONE_W, PHONE_H) != 0) {
                linux_drm_close(g);
                return -1;
            }
        }
        return 0;
    }
#endif
    int fd = open("/dev/dri/card0", O_RDWR | O_CLOEXEC);
    if (fd < 0)
        fd = open("/dev/dri/card1", O_RDWR | O_CLOEXEC);
    if (fd < 0)
        return -1;
    close(fd);
    return -1;
}
#else
int gfx_backend_probe_drm(Gfx *g)
{
    (void)g;
    return -1;
}
#endif

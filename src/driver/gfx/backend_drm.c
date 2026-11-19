#include "gfx_backend.h"
#include "util.h"

#include <stdio.h>
#include <string.h>

#if !defined(PHONE_DESKTOP) && !defined(PHONE_VFB) && !defined(_WIN32)
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <errno.h>

#ifndef DRM_IOCTL_BASE
#define DRM_IOCTL_BASE 'd'
#endif

int gfx_backend_probe_drm(Gfx *g)
{
    if (!g)
        return -1;
    int fd = open("/dev/dri/card0", O_RDWR | O_CLOEXEC);
    if (fd < 0)
        return -1;
    close(fd);
    (void)g;
    return -1;
}
#else
int gfx_backend_probe_drm(Gfx *g)
{
    (void)g;
    return -1;
}
#endif

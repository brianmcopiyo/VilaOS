#ifndef VILAOS_LINUX_DRM_PRIV_H
#define VILAOS_LINUX_DRM_PRIV_H

#include "types.h"
#include <stdint.h>

int linux_drm_fd(const Gfx *g);
uint32_t linux_drm_crtc_id(const Gfx *g);
uint32_t linux_drm_conn_id(const Gfx *g);
int linux_drm_mode_size(const Gfx *g, int *w, int *h);
int linux_drm_add_fb(Gfx *g, uint32_t handle, uint32_t pitch, uint32_t w, uint32_t h, uint32_t *fb_out);
int linux_drm_page_flip(Gfx *g, uint32_t fb_id);

#endif

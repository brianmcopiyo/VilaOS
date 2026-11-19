#ifndef VILAOS_GFX_BACKEND_H
#define VILAOS_GFX_BACKEND_H

#include "types.h"

int gfx_backend_probe_drm(Gfx *g);
int gfx_backend_probe_fb(Gfx *g);
int gfx_backend_probe_sdl(Gfx *g);
int gfx_backend_init_all(Gfx *g, int desktop);
void gfx_backend_present(Gfx *g);
void gfx_backend_shutdown(Gfx *g);
int gfx_backend_poll_desktop(Gfx *g);

#endif

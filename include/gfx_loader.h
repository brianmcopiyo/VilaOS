#ifndef VILAOS_GFX_LOADER_H
#define VILAOS_GFX_LOADER_H

#include "gfx_driver.h"

#define VILAOS_GFX_DRIVERS_ROOT "/drivers/gfx"

int gfx_loader_init(Gfx *g, int desktop);
void gfx_loader_shutdown(Gfx *g);
const vilaos_gfx_driver_t *gfx_loader_driver(const Gfx *g);
void gfx_loader_status(const Gfx *g, char *buf, size_t n);

#endif

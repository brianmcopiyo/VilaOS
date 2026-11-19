#ifndef VILAOS_POOL_H
#define VILAOS_POOL_H

#include "hwinfo.h"

typedef void (*PoolJobFn)(int band_y0, int band_y1, void *user);

typedef struct {
    int workers;
    int ready;
} GfxPool;

int gfx_pool_init(GfxPool *p, const HwProfile *hw);
void gfx_pool_shutdown(GfxPool *p);
void gfx_pool_parallel_bands(GfxPool *p, int y0, int y1, PoolJobFn fn, void *user);

#endif

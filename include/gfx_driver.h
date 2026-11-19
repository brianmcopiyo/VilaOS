#ifndef VILAOS_GFX_DRIVER_H
#define VILAOS_GFX_DRIVER_H

#include "types.h"

#define VILAOS_GFX_ABI_VERSION 1
#define VILAOS_GFX_DRIVER_NAME_MAX 64

#define VILAOS_GFX_CAP_AA       (1u << 0)
#define VILAOS_GFX_CAP_LAYERS   (1u << 1)
#define VILAOS_GFX_CAP_GRADIENT (1u << 2)
#define VILAOS_GFX_CAP_DRM      (1u << 3)
#define VILAOS_GFX_CAP_FB       (1u << 4)
#define VILAOS_GFX_CAP_SDL      (1u << 5)
#define VILAOS_GFX_CAP_THREADS  (1u << 6)

#define VILAOS_GFX_FMT_RGB565   1
#define VILAOS_GFX_FMT_XRGB8888 2
#define VILAOS_GFX_FMT_ARGB8888 3

typedef struct {
    unsigned abi_version;
    char name[VILAOS_GFX_DRIVER_NAME_MAX];
    char version[32];
    unsigned caps;
    int max_w;
    int max_h;
    int work_format;
    int present_format;
    int max_threads;
} vilaos_gfx_caps_t;

typedef struct vilaos_gfx_driver vilaos_gfx_driver_t;

struct vilaos_gfx_driver {
    unsigned abi_version;
    const char *name;
    const char *version;
    int (*init)(Gfx *g, int desktop);
    int (*query_caps)(Gfx *g, vilaos_gfx_caps_t *out);
    uint32_t *(*acquire_surface)(Gfx *g, int *w, int *h, int *stride_px);
    void (*present)(Gfx *g);
    int (*resize)(Gfx *g, int w, int h);
    void (*shutdown)(Gfx *g);
};

const vilaos_gfx_driver_t *vilaos_gfx_builtin_driver(void);

#endif

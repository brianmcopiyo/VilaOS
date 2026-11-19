#ifndef VILAOS_HWINFO_H
#define VILAOS_HWINFO_H

#include <stddef.h>

typedef enum {
    HW_TIER_256 = 0,
    HW_TIER_512,
    HW_TIER_2G,
    HW_TIER_8G,
    HW_TIER_32G
} HwRamTier;

typedef struct {
    int cores;
    long ram_kb;
    long ram_mb;
    HwRamTier tier;
    int workers;
    size_t glyph_cache_bytes;
    size_t icon_cache_bytes;
    size_t surface_pool_bytes;
    size_t cmd_buf_bytes;
    int tile_dirty;
    int dual_buffer;
} HwProfile;

void hwinfo_detect(HwProfile *out);
const char *hwinfo_tier_name(HwRamTier tier);

#endif

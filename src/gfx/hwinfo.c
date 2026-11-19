#include "hwinfo.h"
#include "util.h"

#include <stdio.h>
#include <string.h>

#ifndef _WIN32
#include <unistd.h>
#endif

void hwinfo_detect(HwProfile *out)
{
    if (!out)
        return;
    memset(out, 0, sizeof(*out));
    out->cores = 1;
#ifndef _WIN32
    long c = sysconf(_SC_NPROCESSORS_ONLN);
    if (c > 0)
        out->cores = (int)c;
#endif
    long total_kb = 256 * 1024;
    long free_kb = 0, avail_kb = 0;
    if (read_meminfo_kb(&total_kb, &free_kb, &avail_kb) != 0 || total_kb <= 0)
        total_kb = 256 * 1024;
    out->ram_kb = total_kb;
    out->ram_mb = total_kb / 1024;
    if (out->ram_mb < 256)
        out->ram_mb = 256;

    if (out->ram_mb >= 32768) {
        out->tier = HW_TIER_32G;
        out->workers = out->cores < 16 ? out->cores : 16;
        out->glyph_cache_bytes = 64u * 1024u * 1024u;
        out->surface_pool_bytes = 128u * 1024u * 1024u;
        out->cmd_buf_bytes = 1024u * 1024u;
        out->tile_dirty = 1;
        out->dual_buffer = 1;
    } else if (out->ram_mb >= 8192) {
        out->tier = HW_TIER_8G;
        out->workers = out->cores < 16 ? out->cores : 16;
        out->glyph_cache_bytes = 16u * 1024u * 1024u;
        out->surface_pool_bytes = 48u * 1024u * 1024u;
        out->cmd_buf_bytes = 512u * 1024u;
        out->tile_dirty = 1;
        out->dual_buffer = 1;
    } else if (out->ram_mb >= 2048) {
        out->tier = HW_TIER_2G;
        out->workers = out->cores < 8 ? out->cores : 8;
        out->glyph_cache_bytes = 4u * 1024u * 1024u;
        out->surface_pool_bytes = 16u * 1024u * 1024u;
        out->cmd_buf_bytes = 128u * 1024u;
        out->tile_dirty = 1;
        out->dual_buffer = 1;
    } else if (out->ram_mb >= 512) {
        out->tier = HW_TIER_512;
        out->workers = out->cores < 4 ? out->cores : 4;
        out->glyph_cache_bytes = 1024u * 1024u;
        out->surface_pool_bytes = 4u * 1024u * 1024u;
        out->cmd_buf_bytes = 64u * 1024u;
        out->tile_dirty = 1;
        out->dual_buffer = 0;
    } else {
        out->tier = HW_TIER_256;
        out->workers = out->cores > 1 ? 2 : 1;
        out->glyph_cache_bytes = 256u * 1024u;
        out->surface_pool_bytes = 1024u * 1024u;
        out->cmd_buf_bytes = 32u * 1024u;
        out->tile_dirty = 0;
        out->dual_buffer = 0;
    }
    if (out->workers < 1)
        out->workers = 1;
}

const char *hwinfo_tier_name(HwRamTier tier)
{
    switch (tier) {
    case HW_TIER_256:
        return "256MB";
    case HW_TIER_512:
        return "512MB";
    case HW_TIER_2G:
        return "2GB";
    case HW_TIER_8G:
        return "8GB";
    case HW_TIER_32G:
        return "32GB+";
    default:
        return "256MB";
    }
}

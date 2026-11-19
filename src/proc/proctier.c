#include "proctier.h"
#include "util.h"

#include <stdio.h>
#include <string.h>

void proctier_detect(ProcTier *out)
{
    if (!out)
        return;
    memset(out, 0, sizeof(*out));
    hwinfo_detect(&out->hw);
    switch (out->hw.tier) {
    case HW_TIER_32G:
        out->svc_workers = out->hw.cores < 16 ? out->hw.cores : 16;
        out->ipc_queue = 64;
        out->ipc_buf = 4096;
        out->track_buf = 256 * 1024;
        break;
    case HW_TIER_8G:
        out->svc_workers = out->hw.cores < 16 ? out->hw.cores : 16;
        out->ipc_queue = 48;
        out->ipc_buf = 2048;
        out->track_buf = 128 * 1024;
        break;
    case HW_TIER_2G:
        out->svc_workers = out->hw.cores < 8 ? out->hw.cores : 8;
        out->ipc_queue = 32;
        out->ipc_buf = 1024;
        out->track_buf = 64 * 1024;
        break;
    case HW_TIER_512:
        out->svc_workers = out->hw.cores < 4 ? out->hw.cores : 4;
        out->ipc_queue = 16;
        out->ipc_buf = 512;
        out->track_buf = 32 * 1024;
        break;
    case HW_TIER_256:
    default:
        out->svc_workers = out->hw.cores > 1 ? 2 : 1;
        out->ipc_queue = 8;
        out->ipc_buf = 256;
        out->track_buf = 8 * 1024;
        break;
    }
    if (out->svc_workers < 1)
        out->svc_workers = 1;
}

const char *proctier_label(const ProcTier *t)
{
    static char buf[64];
    if (!t)
        return "256MB";
    snprintf(buf, sizeof(buf), "%s/%dc/%dw",
             hwinfo_tier_name(t->hw.tier), t->hw.cores, t->svc_workers);
    return buf;
}

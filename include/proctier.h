#ifndef VILAOS_PROCTIER_H
#define VILAOS_PROCTIER_H

#include "hwinfo.h"

typedef struct {
    HwProfile hw;
    int svc_workers;
    int ipc_queue;
    size_t ipc_buf;
    size_t track_buf;
} ProcTier;

void proctier_detect(ProcTier *out);
const char *proctier_label(const ProcTier *t);

#endif

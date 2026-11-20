#ifndef VILAOS_LINUX_GPU_H
#define VILAOS_LINUX_GPU_H

#include "types.h"

int linux_gpu_probe(char *name, size_t name_n, long *vram_mb);
int linux_gpu_open(Gfx *g);
void linux_gpu_close(Gfx *g);
int linux_gpu_present(Gfx *g);
int linux_gpu_resize(Gfx *g, int w, int h);
int linux_gpu_poll_hotplug(Gfx *g);
int linux_gpu_active(const Gfx *g);
const char *linux_gpu_tier_name(int tier);

#endif

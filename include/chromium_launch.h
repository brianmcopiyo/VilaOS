#ifndef VILAOS_CHROMIUM_LAUNCH_H
#define VILAOS_CHROMIUM_LAUNCH_H

#include "types.h"

int chromium_available(void);
int chromium_launch(const char *url);
int chromium_running(void);
void chromium_reap(void);
int chromium_release_display(Gfx *g);

#endif

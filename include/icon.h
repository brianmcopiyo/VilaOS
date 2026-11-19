#ifndef VILAOS_ICON_H
#define VILAOS_ICON_H

#include "types.h"

void icon_init(Gfx *g);
void icon_shutdown(void);
int icon_draw(Gfx *g, int cx, int cy, int size, const char *name, const char *set, uint16_t fg);
const char *icon_alias(const char *name, const char *set);

#endif

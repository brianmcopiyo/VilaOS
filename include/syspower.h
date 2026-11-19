#ifndef VILAOS_SYSPOWER_H
#define VILAOS_SYSPOWER_H

#include "types.h"
#include "input.h"

void syspower_show_message(Screen *s, Gfx *g, InputMux *input, const char *msg);
void syspower_off(Screen *s, Gfx *g, InputMux *input);
void syspower_restart(Screen *s, Gfx *g, InputMux *input);

#endif

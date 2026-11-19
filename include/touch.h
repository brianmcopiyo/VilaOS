#ifndef VILAOS_TOUCH_H
#define VILAOS_TOUCH_H

#include "input.h"
#include "screen.h"
#include "fb.h"

int screen_find_scrollable(Screen *s, int x, int y);
int screen_apply_scroll(Screen *s, Gfx *g, int wi, int dy);
int screen_handle_touch(Screen *s, Gfx *g, InputMux *m, const InputEvent *ev, char *out_nav, size_t out_sz);
int screen_handle_pointer_tap(Screen *s, Gfx *g, int x, int y, char *out_nav, size_t out_sz);

#endif

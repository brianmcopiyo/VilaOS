#ifndef PHONE_SCREEN_H
#define PHONE_SCREEN_H

#include "types.h"

int screen_init(Screen *s);
int screen_load(Screen *s, const char *path);
void screen_layout(Screen *s, Gfx *g);
void screen_render(Screen *s, Gfx *g, const InputState *in);
Widget *screen_widget(Screen *s, int idx);
Widget *screen_find(Screen *s, const char *id);
int screen_focus_first(Screen *s);
int screen_focus_next(Screen *s, int dir);
int screen_focus_at(Screen *s, int x, int y);
int screen_hit_softkey(const Screen *s, const Gfx *g, int x, int y);
void screen_set_focus(Screen *s, int idx);
void screen_tick(Screen *s, Gfx *g);
void screen_carry_text(const char *id, const char *text);
void screen_set_mode_badge(Screen *s, const char *mode);
int screen_handle_action(Screen *s, const char *action, char *out_nav, size_t out_sz);

#endif

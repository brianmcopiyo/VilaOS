#include "compositor.h"

#include <string.h>

int compositor_start(DisplayCompositor *c, Gfx *gfx, Screen *screen, WindowManager *wm, InputMux *input)
{
    (void)gfx;
    (void)screen;
    (void)wm;
    (void)input;
    if (!c)
        return -1;
    memset(c, 0, sizeof(*c));
    return -1;
}

void compositor_stop(DisplayCompositor *c)
{
    if (c)
        c->running = 0;
}

void compositor_lock(DisplayCompositor *c)
{
    (void)c;
}

void compositor_unlock(DisplayCompositor *c)
{
    (void)c;
}

void compositor_request(DisplayCompositor *c)
{
    (void)c;
}

void compositor_request_full(DisplayCompositor *c)
{
    (void)c;
}

void compositor_note_input(DisplayCompositor *c)
{
    (void)c;
}

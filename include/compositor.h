#ifndef VILAOS_COMPOSITOR_H
#define VILAOS_COMPOSITOR_H

#include "types.h"
#include "screen.h"
#include "input.h"
#include "fb.h"
#include "window.h"

#define COMPOSITOR_CURSOR_W 16
#define COMPOSITOR_CURSOR_H 16

typedef struct {
    Gfx *gfx;
    Screen *screen;
    WindowManager *wm;
    InputMux *input;
    pthread_mutex_t mu;
    pthread_cond_t cv;
    pthread_t thread;
    int running;
    int dirty;
    int content_dirty;
    int cursor_dirty;
    int force_full;
    int last_sec;
    int cursor_x;
    int cursor_y;
    int cursor_on;
    int prev_cursor_x;
    int prev_cursor_y;
    int prev_cursor_on;
    uint32_t pointer_gen;
    uint16_t cursor_under[COMPOSITOR_CURSOR_W * COMPOSITOR_CURSOR_H];
    int under_x;
    int under_y;
    int under_valid;
} DisplayCompositor;

int compositor_start(DisplayCompositor *c, Gfx *gfx, Screen *screen, WindowManager *wm, InputMux *input);
void compositor_stop(DisplayCompositor *c);
void compositor_lock(DisplayCompositor *c);
void compositor_unlock(DisplayCompositor *c);
void compositor_request(DisplayCompositor *c);
void compositor_request_full(DisplayCompositor *c);
void compositor_note_input(DisplayCompositor *c);

#endif

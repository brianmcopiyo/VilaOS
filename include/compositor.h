#ifndef VILAOS_COMPOSITOR_H
#define VILAOS_COMPOSITOR_H

#include "types.h"
#include "screen.h"
#include "input.h"
#include "fb.h"

#include <pthread.h>

typedef struct {
    Gfx *gfx;
    Screen *screen;
    InputMux *input;
    pthread_mutex_t mu;
    pthread_cond_t cv;
    pthread_t thread;
    int running;
    int dirty;
    int force_full;
    int last_sec;
    int cursor_x;
    int cursor_y;
    int cursor_on;
} DisplayCompositor;

int compositor_start(DisplayCompositor *c, Gfx *gfx, Screen *screen, InputMux *input);
void compositor_stop(DisplayCompositor *c);
void compositor_lock(DisplayCompositor *c);
void compositor_unlock(DisplayCompositor *c);
void compositor_request(DisplayCompositor *c);
void compositor_request_full(DisplayCompositor *c);
void compositor_note_input(DisplayCompositor *c);

#endif

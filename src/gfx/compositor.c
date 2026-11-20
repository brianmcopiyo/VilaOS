#include "compositor.h"
#include "draw.h"
#include "util.h"

#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

#define FRAME_NS 16666667LL

static int64_t mono_ns(void)
{
#ifdef _WIN32
    return (int64_t)now_ms() * 1000000LL;
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (int64_t)ts.tv_sec * 1000000000LL + (int64_t)ts.tv_nsec;
#endif
}

static void sleep_ns(int64_t ns)
{
    if (ns <= 0)
        return;
#ifdef _WIN32
    uint64_t ms = (uint64_t)(ns / 1000000LL);
    if (ms > 0)
        Sleep((DWORD)ms);
#else
    struct timespec ts;
    ts.tv_sec = (time_t)(ns / 1000000000LL);
    ts.tv_nsec = (long)(ns % 1000000000LL);
    while (nanosleep(&ts, &ts) == EINTR)
        ;
#endif
}

static int clock_second(void)
{
    return (int)(time(NULL) % 60);
}

static void read_pointer(const InputMux *input, int *x, int *y, int *on, uint32_t *gen)
{
    *x = 0;
    *y = 0;
    *on = 0;
    *gen = 0;
    if (!input)
        return;
    if (input->pump_started) {
        *x = input->coalesce_x;
        *y = input->coalesce_y;
        *on = input->coalesce_vis;
        *gen = input->pointer_gen;
    } else {
        *x = input->state.cursor_x;
        *y = input->state.cursor_y;
        *on = input->state.cursor_visible;
        *gen = input->pointer_gen;
    }
}

static void save_underlay(DisplayCompositor *c, Gfx *gfx, int x, int y)
{
    if (!c || !gfx || !gfx->back)
        return;
    for (int row = 0; row < COMPOSITOR_CURSOR_H; row++) {
        int sy = y + row;
        for (int col = 0; col < COMPOSITOR_CURSOR_W; col++) {
            int sx = x + col;
            uint16_t pix = COL_BG;
            if (sx >= 0 && sy >= 0 && sx < gfx->w && sy < gfx->h)
                pix = gfx->back[sy * gfx->w + sx];
            c->cursor_under[row * COMPOSITOR_CURSOR_W + col] = pix;
        }
    }
    c->under_x = x;
    c->under_y = y;
    c->under_valid = 1;
}

static void restore_underlay(DisplayCompositor *c, Gfx *gfx)
{
    if (!c || !gfx || !gfx->back || !c->under_valid)
        return;
    int ox = c->under_x;
    int oy = c->under_y;
    for (int row = 0; row < COMPOSITOR_CURSOR_H; row++) {
        int sy = oy + row;
        for (int col = 0; col < COMPOSITOR_CURSOR_W; col++) {
            int sx = ox + col;
            if (sx < 0 || sy < 0 || sx >= gfx->w || sy >= gfx->h)
                continue;
            gfx->back[sy * gfx->w + sx] = c->cursor_under[row * COMPOSITOR_CURSOR_W + col];
        }
    }
    gfx_dirty(gfx, ox, oy, COMPOSITOR_CURSOR_W, COMPOSITOR_CURSOR_H);
    c->under_valid = 0;
}

static void paint_cursor(Gfx *gfx, int x, int y)
{
    if (!gfx)
        return;
    draw_line(gfx, x, y, x + 8, y + 8, COL_ACCENT);
    draw_line(gfx, x, y, x, y + 12, COL_ACCENT);
    draw_line(gfx, x, y + 12, x + 4, y + 9, COL_ACCENT);
    draw_line(gfx, x + 4, y + 9, x + 8, y + 8, COL_ACCENT);
    draw_pixel(gfx, x + 1, y + 1, COL_TEXT);
    gfx_dirty(gfx, x, y, COMPOSITOR_CURSOR_W, COMPOSITOR_CURSOR_H);
}

static void *compositor_main(void *arg)
{
    DisplayCompositor *c = (DisplayCompositor *)arg;
    int64_t next_frame = mono_ns();
    while (1) {
        pthread_mutex_lock(&c->mu);
        while (c->running && !c->dirty) {
            struct timespec ts;
            int64_t wake = mono_ns() + FRAME_NS;
            ts.tv_sec = (time_t)(wake / 1000000000LL);
            ts.tv_nsec = (long)(wake % 1000000000LL);
#ifdef _WIN32
            (void)ts;
            pthread_cond_wait(&c->cv, &c->mu);
#else
            int rc = pthread_cond_timedwait(&c->cv, &c->mu, &ts);
            if (rc == ETIMEDOUT) {
                int sec = clock_second();
                if (sec != c->last_sec) {
                    c->last_sec = sec;
                    c->content_dirty = 1;
                    c->dirty = 1;
                }
                int cx, cy, on;
                uint32_t gen;
                read_pointer(c->input, &cx, &cy, &on, &gen);
                if (gen != c->pointer_gen || cx != c->cursor_x || cy != c->cursor_y || on != c->cursor_on) {
                    c->cursor_x = cx;
                    c->cursor_y = cy;
                    c->cursor_on = on;
                    c->pointer_gen = gen;
                    c->cursor_dirty = 1;
                    c->dirty = 1;
                }
            }
#endif
        }
        if (!c->running) {
            pthread_mutex_unlock(&c->mu);
            break;
        }

        int force = c->force_full;
        int do_content = c->content_dirty || force;
        int do_cursor = c->cursor_dirty || do_content;
        c->dirty = 0;
        c->content_dirty = 0;
        c->cursor_dirty = 0;
        c->force_full = 0;

        Gfx *gfx = c->gfx;
        Screen *screen = c->screen;
        InputMux *input = c->input;
        int cx = c->cursor_x;
        int cy = c->cursor_y;
        int on = c->cursor_on;
        int prev_x = c->prev_cursor_x;
        int prev_y = c->prev_cursor_y;
        int prev_on = c->prev_cursor_on;

        if (do_content && screen && gfx) {
            if (force)
                gfx_dirty_all(gfx);
            InputState blank;
            memset(&blank, 0, sizeof(blank));
            if (input)
                blank = input->state;
            blank.cursor_visible = 0;
            screen_render(screen, gfx, &blank);
            c->under_valid = 0;
            if (on) {
                save_underlay(c, gfx, cx, cy);
                paint_cursor(gfx, cx, cy);
            }
            c->prev_cursor_x = cx;
            c->prev_cursor_y = cy;
            c->prev_cursor_on = on;
            pthread_mutex_unlock(&c->mu);
            gfx_present(gfx);
        } else if (do_cursor && gfx) {
            if (prev_on || c->under_valid)
                restore_underlay(c, gfx);
            if (prev_on)
                gfx_dirty(gfx, prev_x, prev_y, COMPOSITOR_CURSOR_W, COMPOSITOR_CURSOR_H);
            if (on) {
                save_underlay(c, gfx, cx, cy);
                paint_cursor(gfx, cx, cy);
            }
            c->prev_cursor_x = cx;
            c->prev_cursor_y = cy;
            c->prev_cursor_on = on;
            pthread_mutex_unlock(&c->mu);
            gfx_present(gfx);
        } else {
            pthread_mutex_unlock(&c->mu);
        }

        int64_t now = mono_ns();
        next_frame += FRAME_NS;
        if (next_frame < now - FRAME_NS * 2)
            next_frame = now + FRAME_NS;
        if (next_frame > now)
            sleep_ns(next_frame - now);
        else
            next_frame = now;
    }
    return NULL;
}

int compositor_start(DisplayCompositor *c, Gfx *gfx, Screen *screen, InputMux *input)
{
    if (!c || !gfx || !screen)
        return -1;
    memset(c, 0, sizeof(*c));
    c->gfx = gfx;
    c->screen = screen;
    c->input = input;
    c->running = 1;
    c->dirty = 1;
    c->content_dirty = 1;
    c->force_full = 1;
    c->last_sec = clock_second();
    if (input)
        read_pointer(input, &c->cursor_x, &c->cursor_y, &c->cursor_on, &c->pointer_gen);
    c->prev_cursor_x = c->cursor_x;
    c->prev_cursor_y = c->cursor_y;
    c->prev_cursor_on = 0;
    pthread_mutexattr_t mattr;
    pthread_mutexattr_init(&mattr);
    pthread_mutexattr_settype(&mattr, PTHREAD_MUTEX_RECURSIVE);
    if (pthread_mutex_init(&c->mu, &mattr) != 0) {
        pthread_mutexattr_destroy(&mattr);
        return -1;
    }
    pthread_mutexattr_destroy(&mattr);
    {
        pthread_condattr_t attr;
        pthread_condattr_init(&attr);
#ifndef _WIN32
        pthread_condattr_setclock(&attr, CLOCK_MONOTONIC);
#endif
        if (pthread_cond_init(&c->cv, &attr) != 0) {
            pthread_condattr_destroy(&attr);
            pthread_mutex_destroy(&c->mu);
            return -1;
        }
        pthread_condattr_destroy(&attr);
    }
    if (pthread_create(&c->thread, NULL, compositor_main, c) != 0) {
        pthread_cond_destroy(&c->cv);
        pthread_mutex_destroy(&c->mu);
        c->running = 0;
        return -1;
    }
    return 0;
}

void compositor_stop(DisplayCompositor *c)
{
    if (!c || !c->running)
        return;
    pthread_mutex_lock(&c->mu);
    c->running = 0;
    c->dirty = 1;
    pthread_cond_broadcast(&c->cv);
    pthread_mutex_unlock(&c->mu);
    pthread_join(c->thread, NULL);
    pthread_cond_destroy(&c->cv);
    pthread_mutex_destroy(&c->mu);
    c->gfx = NULL;
    c->screen = NULL;
    c->input = NULL;
}

void compositor_lock(DisplayCompositor *c)
{
    if (c)
        pthread_mutex_lock(&c->mu);
}

void compositor_unlock(DisplayCompositor *c)
{
    if (c)
        pthread_mutex_unlock(&c->mu);
}

void compositor_request(DisplayCompositor *c)
{
    if (!c)
        return;
    pthread_mutex_lock(&c->mu);
    c->dirty = 1;
    c->content_dirty = 1;
    pthread_cond_signal(&c->cv);
    pthread_mutex_unlock(&c->mu);
}

void compositor_request_full(DisplayCompositor *c)
{
    if (!c)
        return;
    pthread_mutex_lock(&c->mu);
    c->dirty = 1;
    c->content_dirty = 1;
    c->force_full = 1;
    pthread_cond_signal(&c->cv);
    pthread_mutex_unlock(&c->mu);
}

void compositor_note_input(DisplayCompositor *c)
{
    if (!c || !c->input)
        return;
    int cx, cy, on;
    uint32_t gen;
    read_pointer(c->input, &cx, &cy, &on, &gen);
    pthread_mutex_lock(&c->mu);
    if (gen != c->pointer_gen || cx != c->cursor_x || cy != c->cursor_y || on != c->cursor_on) {
        c->cursor_x = cx;
        c->cursor_y = cy;
        c->cursor_on = on;
        c->pointer_gen = gen;
        c->cursor_dirty = 1;
        c->dirty = 1;
        pthread_cond_signal(&c->cv);
    }
    pthread_mutex_unlock(&c->mu);
}

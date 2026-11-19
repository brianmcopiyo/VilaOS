#include "compositor.h"
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
                    c->dirty = 1;
                }
                if (c->input) {
                    int cx = c->input->state.cursor_x;
                    int cy = c->input->state.cursor_y;
                    int on = c->input->state.cursor_visible;
                    if (cx != c->cursor_x || cy != c->cursor_y || on != c->cursor_on) {
                        c->cursor_x = cx;
                        c->cursor_y = cy;
                        c->cursor_on = on;
                        c->dirty = 1;
                    }
                }
            }
#endif
        }
        if (!c->running) {
            pthread_mutex_unlock(&c->mu);
            break;
        }
        c->dirty = 0;
        int force = c->force_full;
        c->force_full = 0;
        Gfx *gfx = c->gfx;
        Screen *screen = c->screen;
        InputMux *input = c->input;
        if (force && gfx)
            gfx_dirty_all(gfx);
        if (screen && gfx)
            screen_render(screen, gfx, input ? &input->state : NULL);
        pthread_mutex_unlock(&c->mu);

        if (gfx)
            gfx_present(gfx);

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
    c->force_full = 1;
    c->last_sec = clock_second();
    if (input) {
        c->cursor_x = input->state.cursor_x;
        c->cursor_y = input->state.cursor_y;
        c->cursor_on = input->state.cursor_visible;
    }
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
    pthread_cond_signal(&c->cv);
    pthread_mutex_unlock(&c->mu);
}

void compositor_request_full(DisplayCompositor *c)
{
    if (!c)
        return;
    pthread_mutex_lock(&c->mu);
    c->dirty = 1;
    c->force_full = 1;
    pthread_cond_signal(&c->cv);
    pthread_mutex_unlock(&c->mu);
}

void compositor_note_input(DisplayCompositor *c)
{
    if (!c || !c->input)
        return;
    int cx = c->input->state.cursor_x;
    int cy = c->input->state.cursor_y;
    int on = c->input->state.cursor_visible;
    pthread_mutex_lock(&c->mu);
    int changed = (cx != c->cursor_x || cy != c->cursor_y || on != c->cursor_on);
    if (changed) {
        c->cursor_x = cx;
        c->cursor_y = cy;
        c->cursor_on = on;
        c->dirty = 1;
        pthread_cond_signal(&c->cv);
    }
    pthread_mutex_unlock(&c->mu);
}

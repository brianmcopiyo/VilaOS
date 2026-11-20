#include "input_pump.h"
#include "util.h"

#include <errno.h>
#include <string.h>
#include <time.h>

#ifndef _WIN32
#include <unistd.h>
#endif

static void enqueue_locked(InputMux *m, const InputEvent *ev)
{
    if (!m || !ev || ev->kind == IE_NONE)
        return;
    if (ev->kind == IE_TOUCH_MOVE && m->q_count > 0) {
        int last = (m->q_tail + INPUT_Q_MAX - 1) % INPUT_Q_MAX;
        if (m->q[last].kind == IE_TOUCH_MOVE) {
            m->q[last] = *ev;
            return;
        }
    }
    if (m->q_count >= INPUT_Q_MAX) {
        m->q_head = (m->q_head + 1) % INPUT_Q_MAX;
        m->q_count--;
    }
    m->q[m->q_tail] = *ev;
    m->q_tail = (m->q_tail + 1) % INPUT_Q_MAX;
    m->q_count++;
}

static int dequeue_locked(InputMux *m, InputEvent *out)
{
    if (!m || !out || m->q_count <= 0)
        return 0;
    *out = m->q[m->q_head];
    m->q_head = (m->q_head + 1) % INPUT_Q_MAX;
    m->q_count--;
    return 1;
}

static void *input_pump_main(void *arg)
{
    InputMux *m = (InputMux *)arg;
    while (1) {
        pthread_mutex_lock(&m->pump_mu);
        int run = m->pump_run;
        int before_x = m->state.cursor_x;
        int before_y = m->state.cursor_y;
        int before_vis = m->state.cursor_visible;
        uint32_t before_gen = m->pointer_gen;
        pthread_mutex_unlock(&m->pump_mu);
        if (!run)
            break;

        InputEvent ev;
        memset(&ev, 0, sizeof(ev));
        int got = input_dev_read(m, &ev, 8);

        pthread_mutex_lock(&m->pump_mu);
        int moved = (m->state.cursor_x != before_x || m->state.cursor_y != before_y ||
                     m->state.cursor_visible != before_vis);
        if (moved) {
            m->coalesce_x = m->state.cursor_x;
            m->coalesce_y = m->state.cursor_y;
            m->coalesce_vis = m->state.cursor_visible;
            m->pointer_gen = before_gen + 1;
        }
        if (got && ev.kind != IE_NONE) {
            if (ev.kind == IE_TOUCH_MOVE) {
                m->coalesce_x = ev.x;
                m->coalesce_y = ev.y;
                m->coalesce_vis = 1;
                m->pointer_gen++;
            }
            enqueue_locked(m, &ev);
            pthread_cond_broadcast(&m->pump_cv);
        } else if (moved) {
            pthread_cond_broadcast(&m->pump_cv);
        }
        pthread_mutex_unlock(&m->pump_mu);
    }
    return NULL;
}

int input_pump_start(InputMux *m)
{
    if (!m || m->pump_started)
        return 0;
    if (pthread_mutex_init(&m->pump_mu, NULL) != 0)
        return -1;
    if (pthread_cond_init(&m->pump_cv, NULL) != 0) {
        pthread_mutex_destroy(&m->pump_mu);
        return -1;
    }
    m->pump_run = 1;
    m->q_head = m->q_tail = m->q_count = 0;
    m->pointer_gen = 1;
    m->coalesce_x = m->state.cursor_x;
    m->coalesce_y = m->state.cursor_y;
    m->coalesce_vis = m->state.cursor_visible;
    if (pthread_create(&m->pump_thr, NULL, input_pump_main, m) != 0) {
        m->pump_run = 0;
        pthread_cond_destroy(&m->pump_cv);
        pthread_mutex_destroy(&m->pump_mu);
        return -1;
    }
    m->pump_started = 1;
    return 0;
}

void input_pump_stop(InputMux *m)
{
    if (!m || !m->pump_started)
        return;
    pthread_mutex_lock(&m->pump_mu);
    m->pump_run = 0;
    pthread_cond_broadcast(&m->pump_cv);
    pthread_mutex_unlock(&m->pump_mu);
    pthread_join(m->pump_thr, NULL);
    pthread_cond_destroy(&m->pump_cv);
    pthread_mutex_destroy(&m->pump_mu);
    m->pump_started = 0;
    m->q_count = 0;
}

int input_pump_pop(InputMux *m, InputEvent *out, int timeout_ms)
{
    if (!m || !out)
        return 0;
    memset(out, 0, sizeof(*out));
    pthread_mutex_lock(&m->pump_mu);
    if (m->q_count <= 0 && timeout_ms > 0) {
        struct timespec ts;
        clock_gettime(CLOCK_REALTIME, &ts);
        ts.tv_nsec += (long)timeout_ms * 1000000L;
        if (ts.tv_nsec >= 1000000000L) {
            ts.tv_sec += ts.tv_nsec / 1000000000L;
            ts.tv_nsec %= 1000000000L;
        }
        while (m->q_count <= 0 && m->pump_run) {
            int rc = pthread_cond_timedwait(&m->pump_cv, &m->pump_mu, &ts);
            if (rc == ETIMEDOUT || rc != 0)
                break;
        }
    }
    if (m->coalesce_vis && now_ms() - m->state.last_pointer_ms > 4000 &&
        now_ms() - m->state.last_key_ms < 4000) {
        m->coalesce_vis = 0;
        m->state.cursor_visible = 0;
        m->pointer_gen++;
    }
    m->state.cursor_x = m->coalesce_x;
    m->state.cursor_y = m->coalesce_y;
    m->state.cursor_visible = m->coalesce_vis;
    int got = dequeue_locked(m, out);
    pthread_mutex_unlock(&m->pump_mu);
    return got;
}

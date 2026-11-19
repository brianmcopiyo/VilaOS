#include "ipc.h"
#include "util.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <signal.h>

#ifndef _WIN32
#include <unistd.h>
#include <pthread.h>
#include <poll.h>
#endif

typedef struct {
    char title[64];
    int len_sec;
} Track;

static Track g_tracks[] = {
    {"Sunset Drive", 180},
    {"Warm Beige Nights", 210},
    {"Orange Glow", 165},
    {"Idle Loop", 240}
};
static const int g_track_count = 4;

static volatile int g_run = 1;
static int g_playing;
static int g_track;
static int g_pos;
static int g_workers = 1;
static int g_queue = 8;
static char g_sock[64] = "music.sock";
static int g_listen = -1;

#ifndef _WIN32
static pthread_mutex_t g_mu = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t g_cv = PTHREAD_COND_INITIALIZER;
static pthread_t g_tick_th;
static pthread_t *g_worker_th;
static int g_worker_n;
static volatile unsigned long g_work_ticks;

static void *tick_main(void *arg)
{
    (void)arg;
    while (g_run) {
        pthread_mutex_lock(&g_mu);
        while (g_run && !g_playing)
            pthread_cond_wait(&g_cv, &g_mu);
        int playing = g_playing;
        int track = g_track;
        int len = g_tracks[track].len_sec;
        pthread_mutex_unlock(&g_mu);
        if (!g_run)
            break;
        if (!playing) {
            usleep(50000);
            continue;
        }
        usleep(200000);
        pthread_mutex_lock(&g_mu);
        if (g_playing) {
            g_pos += 1;
            if (g_pos >= len) {
                g_pos = 0;
                g_track = (g_track + 1) % g_track_count;
            }
        }
        pthread_mutex_unlock(&g_mu);
    }
    return NULL;
}

static void *worker_main(void *arg)
{
    (void)arg;
    while (g_run) {
        pthread_mutex_lock(&g_mu);
        int busy = g_playing;
        pthread_mutex_unlock(&g_mu);
        if (busy)
            g_work_ticks++;
        usleep(busy ? 100000 : 250000);
    }
    return NULL;
}
#endif

static void format_status(char *out, size_t n)
{
#ifndef _WIN32
    pthread_mutex_lock(&g_mu);
#endif
    int playing = g_playing;
    int track = g_track;
    int pos = g_pos;
    int len = g_tracks[track].len_sec;
    const char *title = g_tracks[track].title;
#ifndef _WIN32
    unsigned long wt = g_work_ticks;
    pthread_mutex_unlock(&g_mu);
#else
    unsigned long wt = 0;
#endif
    snprintf(out, n, "OK state=%s track=%d title=%s pos=%d len=%d workers=%d queue=%d work=%lu",
             playing ? "playing" : "paused", track + 1, title, pos, len, g_workers, g_queue, wt);
}

static void handle_cmd(const char *cmd, char *resp, size_t n)
{
    if (!cmd || !cmd[0]) {
        str_copy(resp, n, "ERR empty");
        return;
    }
    if (str_eq(cmd, "PING")) {
        str_copy(resp, n, "OK pong");
        return;
    }
    if (str_eq(cmd, "STATUS")) {
        format_status(resp, n);
        return;
    }
    if (str_eq(cmd, "PLAY")) {
#ifndef _WIN32
        pthread_mutex_lock(&g_mu);
        g_playing = 1;
        pthread_cond_broadcast(&g_cv);
        pthread_mutex_unlock(&g_mu);
#else
        g_playing = 1;
#endif
        format_status(resp, n);
        return;
    }
    if (str_eq(cmd, "PAUSE")) {
#ifndef _WIN32
        pthread_mutex_lock(&g_mu);
        g_playing = 0;
        pthread_mutex_unlock(&g_mu);
#else
        g_playing = 0;
#endif
        format_status(resp, n);
        return;
    }
    if (str_eq(cmd, "SKIP")) {
#ifndef _WIN32
        pthread_mutex_lock(&g_mu);
        g_track = (g_track + 1) % g_track_count;
        g_pos = 0;
        pthread_mutex_unlock(&g_mu);
#else
        g_track = (g_track + 1) % g_track_count;
        g_pos = 0;
#endif
        format_status(resp, n);
        return;
    }
    if (str_eq(cmd, "STOP") || str_eq(cmd, "SHUTDOWN")) {
#ifndef _WIN32
        pthread_mutex_lock(&g_mu);
        g_playing = 0;
        g_run = 0;
        pthread_cond_broadcast(&g_cv);
        pthread_mutex_unlock(&g_mu);
#else
        g_playing = 0;
        g_run = 0;
#endif
        str_copy(resp, n, "OK shutdown");
        return;
    }
    str_copy(resp, n, "ERR unknown");
}

#ifndef _WIN32
static void on_term(int sig)
{
    (void)sig;
    g_run = 0;
    pthread_cond_broadcast(&g_cv);
}
#endif

int main(int argc, char **argv)
{
    for (int i = 1; i < argc; i++) {
        if (str_eq(argv[i], "--sock") && i + 1 < argc)
            str_copy(g_sock, sizeof(g_sock), argv[++i]);
        else if (str_eq(argv[i], "--workers") && i + 1 < argc)
            g_workers = atoi(argv[++i]);
        else if (str_eq(argv[i], "--queue") && i + 1 < argc)
            g_queue = atoi(argv[++i]);
    }
    if (g_workers < 1)
        g_workers = 1;
    if (g_workers > 16)
        g_workers = 16;
    if (g_queue < 4)
        g_queue = 4;

#ifdef _WIN32
    fprintf(stderr, "vilaos-musicd: unsupported on win32\n");
    return 1;
#else
    signal(SIGTERM, on_term);
    signal(SIGINT, on_term);
    signal(SIGPIPE, SIG_IGN);

    g_listen = ipc_listen(g_sock);
    if (g_listen < 0) {
        fprintf(stderr, "musicd: listen failed\n");
        return 1;
    }

    if (pthread_create(&g_tick_th, NULL, tick_main, NULL) != 0) {
        ipc_close(g_listen);
        return 1;
    }
    g_worker_n = g_workers;
    g_worker_th = (pthread_t *)calloc((size_t)g_worker_n, sizeof(pthread_t));
    if (g_worker_th) {
        for (int i = 0; i < g_worker_n; i++)
            pthread_create(&g_worker_th[i], NULL, worker_main, NULL);
    }

    fprintf(stderr, "musicd: ready sock=%s workers=%d queue=%d\n", g_sock, g_workers, g_queue);

    while (g_run) {
        struct pollfd pfd;
        pfd.fd = g_listen;
        pfd.events = POLLIN;
        int pr = poll(&pfd, 1, 200);
        if (pr < 0) {
            if (errno == EINTR)
                continue;
            break;
        }
        if (pr == 0)
            continue;
        int c = ipc_accept(g_listen);
        if (c < 0)
            continue;
        char req[VILAOS_IPC_MAX_MSG];
        char resp[VILAOS_IPC_MAX_MSG];
        if (ipc_recv_line(c, req, sizeof(req)) >= 0) {
            handle_cmd(req, resp, sizeof(resp));
            ipc_send_line(c, resp);
        }
        ipc_close(c);
        if (!g_run)
            break;
    }

    g_run = 0;
    pthread_cond_broadcast(&g_cv);
    pthread_join(g_tick_th, NULL);
    if (g_worker_th) {
        for (int i = 0; i < g_worker_n; i++)
            pthread_join(g_worker_th[i], NULL);
        free(g_worker_th);
    }
    ipc_close(g_listen);
    {
        char path[MAX_PATH];
        if (ipc_sock_path(g_sock, path, sizeof(path)) == 0)
            unlink(path);
    }
    return 0;
#endif
}

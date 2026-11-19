#include "procman.h"
#include "ipc.h"
#include "util.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <signal.h>

#ifndef _WIN32
#include <unistd.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#endif

ProcMan g_procman;

static volatile sig_atomic_t g_child_exited;

#ifndef _WIN32
static void on_sigchld(int sig)
{
    (void)sig;
    g_child_exited = 1;
}
#endif

static void reap_children(ProcMan *pm)
{
#ifndef _WIN32
    int status = 0;
    pid_t pid;
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        for (int i = 0; i < pm->count; i++) {
            ProcService *s = &pm->svcs[i];
            if (s->pid == (int)pid) {
                s->pid = 0;
                s->state = SVC_STOPPED;
                break;
            }
        }
    }
#else
    (void)pm;
#endif
    g_child_exited = 0;
}

ProcService *procman_find(ProcMan *pm, const char *id)
{
    if (!pm || !id)
        return NULL;
    for (int i = 0; i < pm->count; i++) {
        if (str_eq(pm->svcs[i].id, id))
            return &pm->svcs[i];
    }
    return NULL;
}

int procman_register(ProcMan *pm, const char *id, const char *binary, const char *sock, int autostart)
{
    if (!pm || !id || !binary)
        return -1;
    ProcService *s = procman_find(pm, id);
    if (!s) {
        if (pm->count >= PROCMAN_MAX_SVC)
            return -1;
        s = &pm->svcs[pm->count++];
        memset(s, 0, sizeof(*s));
        str_copy(s->id, sizeof(s->id), id);
    }
    str_copy(s->binary, sizeof(s->binary), binary);
    str_copy(s->sock, sizeof(s->sock), sock && sock[0] ? sock : id);
    s->autostart = autostart ? 1 : 0;
    s->declared = 1;
    return 0;
}

int procman_init(ProcMan *pm)
{
    if (!pm)
        return -1;
    memset(pm, 0, sizeof(*pm));
    proctier_detect(&pm->tier);
    ipc_ensure_rundir();
#ifndef _WIN32
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = on_sigchld;
    sa.sa_flags = SA_RESTART | SA_NOCLDSTOP;
    sigaction(SIGCHLD, &sa, NULL);
#endif
    procman_register(pm, "musicd", "/bin/vilaos-musicd", "music.sock", 0);
    pm->ready = 1;
    fprintf(stderr, "procman: tier=%s\n", proctier_label(&pm->tier));
    return 0;
}

void procman_shutdown(ProcMan *pm)
{
    if (!pm)
        return;
    procman_stop_all(pm);
    pm->ready = 0;
}

int procman_start(ProcMan *pm, const char *id)
{
    if (!pm || !id)
        return -1;
    ProcService *s = procman_find(pm, id);
    if (!s)
        return -1;
    if (s->state == SVC_RUNNING && s->pid > 0)
        return 0;
#ifdef _WIN32
    s->state = SVC_RUNNING;
    s->pid = 1;
    return 0;
#else
    if (access(s->binary, X_OK) != 0)
        return -1;
    s->state = SVC_STARTING;
    pid_t pid = fork();
    if (pid < 0) {
        s->state = SVC_STOPPED;
        return -1;
    }
    if (pid == 0) {
        int nullfd = open("/dev/null", O_RDWR);
        if (nullfd >= 0) {
            dup2(nullfd, STDIN_FILENO);
            if (nullfd > 2)
                close(nullfd);
        }
        char workers[16];
        char queue[16];
        snprintf(workers, sizeof(workers), "%d", pm->tier.svc_workers);
        snprintf(queue, sizeof(queue), "%d", pm->tier.ipc_queue);
        char *argv[] = {
            s->binary,
            "--sock", s->sock,
            "--workers", workers,
            "--queue", queue,
            NULL
        };
        execv(s->binary, argv);
        _exit(127);
    }
    s->pid = (int)pid;
    s->state = SVC_RUNNING;
    for (int i = 0; i < 80; i++) {
        usleep(25000);
        char resp[64];
        if (ipc_request(s->sock, "PING", resp, sizeof(resp)) == 0)
            return 0;
        if (kill(pid, 0) != 0) {
            s->pid = 0;
            s->state = SVC_STOPPED;
            return -1;
        }
    }
    if (s->pid > 0) {
        kill(s->pid, SIGTERM);
        waitpid(s->pid, NULL, 0);
    }
    s->pid = 0;
    s->state = SVC_STOPPED;
    return -1;
#endif
}

int procman_stop(ProcMan *pm, const char *id)
{
    if (!pm || !id)
        return -1;
    ProcService *s = procman_find(pm, id);
    if (!s)
        return -1;
    if (s->state == SVC_STOPPED || s->pid <= 0) {
        s->state = SVC_STOPPED;
        s->pid = 0;
        return 0;
    }
    s->state = SVC_STOPPING;
#ifndef _WIN32
    char resp[64];
    ipc_request(s->sock, "SHUTDOWN", resp, sizeof(resp));
    if (s->pid > 0)
        kill(s->pid, SIGTERM);
    for (int i = 0; i < 40; i++) {
        int status = 0;
        pid_t r = waitpid(s->pid, &status, WNOHANG);
        if (r == s->pid || (kill(s->pid, 0) != 0 && errno == ESRCH)) {
            s->pid = 0;
            s->state = SVC_STOPPED;
            return 0;
        }
        usleep(25000);
    }
    if (s->pid > 0) {
        kill(s->pid, SIGKILL);
        waitpid(s->pid, NULL, 0);
    }
#endif
    s->pid = 0;
    s->state = SVC_STOPPED;
    return 0;
}

void procman_stop_all(ProcMan *pm)
{
    if (!pm)
        return;
    for (int i = 0; i < pm->count; i++) {
        if (pm->svcs[i].state != SVC_STOPPED)
            procman_stop(pm, pm->svcs[i].id);
    }
    reap_children(pm);
}

void procman_tick(ProcMan *pm)
{
    if (!pm || !pm->ready)
        return;
    if (g_child_exited)
        reap_children(pm);
    else
        reap_children(pm);
}

int procman_status(ProcMan *pm, const char *id, char *out, size_t n)
{
    if (!out || n == 0)
        return -1;
    out[0] = 0;
    if (!pm || !id)
        return -1;
    ProcService *s = procman_find(pm, id);
    if (!s) {
        str_copy(out, n, "unknown");
        return -1;
    }
    const char *st = "stopped";
    if (s->state == SVC_RUNNING)
        st = "running";
    else if (s->state == SVC_STARTING)
        st = "starting";
    else if (s->state == SVC_STOPPING)
        st = "stopping";
    char ipc[VILAOS_IPC_MAX_MSG];
    ipc[0] = 0;
    if (s->state == SVC_RUNNING)
        ipc_request(s->sock, "STATUS", ipc, sizeof(ipc));
    if (ipc[0])
        snprintf(out, n, "%s pid=%d %s", st, s->pid, ipc);
    else
        snprintf(out, n, "%s pid=%d", st, s->pid);
    return 0;
}

int procman_cmd(ProcMan *pm, const char *id, const char *cmd, char *resp, size_t n)
{
    if (resp && n)
        resp[0] = 0;
    if (!pm || !id || !cmd)
        return -1;
    ProcService *s = procman_find(pm, id);
    if (!s)
        return -1;
    if (s->state != SVC_RUNNING || s->pid <= 0) {
        if (procman_start(pm, id) != 0)
            return -1;
    }
    return ipc_request(s->sock, cmd, resp, n);
}

const ProcTier *procman_tier(const ProcMan *pm)
{
    return pm ? &pm->tier : NULL;
}

#include "ipc.h"
#include "util.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#ifdef _WIN32
int ipc_ensure_rundir(void) { return -1; }
int ipc_sock_path(const char *name, char *out, size_t n)
{
    if (!out || n == 0)
        return -1;
    out[0] = 0;
    (void)name;
    return -1;
}
int ipc_listen(const char *name) { (void)name; return -1; }
int ipc_accept(int listen_fd) { (void)listen_fd; return -1; }
int ipc_connect(const char *name) { (void)name; return -1; }
int ipc_send_line(int fd, const char *line) { (void)fd; (void)line; return -1; }
int ipc_recv_line(int fd, char *buf, size_t n) { (void)fd; if (buf && n) buf[0] = 0; return -1; }
int ipc_request(const char *name, const char *req, char *resp, size_t resp_n)
{
    (void)name;
    (void)req;
    if (resp && resp_n)
        str_copy(resp, resp_n, "ERR unsupported");
    return -1;
}
void ipc_close(int fd) { (void)fd; }
#else
#include <unistd.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <fcntl.h>

int ipc_ensure_rundir(void)
{
    mkdir("/run", 0755);
    if (mkdir(VILAOS_RUN_DIR, 0755) != 0 && errno != EEXIST)
        return -1;
    return 0;
}

int ipc_sock_path(const char *name, char *out, size_t n)
{
    if (!name || !out || n == 0)
        return -1;
    if (strchr(name, '/') || strchr(name, '\\'))
        return -1;
    snprintf(out, n, "%s/%s", VILAOS_RUN_DIR, name);
    return 0;
}

int ipc_listen(const char *name)
{
    char path[MAX_PATH];
    if (ipc_ensure_rundir() != 0)
        return -1;
    if (ipc_sock_path(name, path, sizeof(path)) != 0)
        return -1;
    unlink(path);
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0)
        return -1;
    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    str_copy(addr.sun_path, sizeof(addr.sun_path), path);
    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
        close(fd);
        return -1;
    }
    if (listen(fd, 8) != 0) {
        close(fd);
        unlink(path);
        return -1;
    }
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags >= 0)
        fcntl(fd, F_SETFL, flags | O_NONBLOCK);
    return fd;
}

int ipc_accept(int listen_fd)
{
    if (listen_fd < 0)
        return -1;
    int c = accept(listen_fd, NULL, NULL);
    if (c < 0)
        return -1;
    return c;
}

int ipc_connect(const char *name)
{
    char path[MAX_PATH];
    if (ipc_sock_path(name, path, sizeof(path)) != 0)
        return -1;
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0)
        return -1;
    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    str_copy(addr.sun_path, sizeof(addr.sun_path), path);
    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
        close(fd);
        return -1;
    }
    return fd;
}

int ipc_send_line(int fd, const char *line)
{
    if (fd < 0 || !line)
        return -1;
    size_t n = strlen(line);
    char buf[VILAOS_IPC_MAX_MSG];
    if (n + 2 > sizeof(buf))
        n = sizeof(buf) - 2;
    memcpy(buf, line, n);
    buf[n++] = '\n';
    buf[n] = 0;
    size_t sent = 0;
    while (sent < n) {
        ssize_t w = write(fd, buf + sent, n - sent);
        if (w < 0) {
            if (errno == EINTR)
                continue;
            return -1;
        }
        sent += (size_t)w;
    }
    return 0;
}

int ipc_recv_line(int fd, char *buf, size_t n)
{
    if (fd < 0 || !buf || n == 0)
        return -1;
    size_t i = 0;
    while (i + 1 < n) {
        char c;
        ssize_t r = read(fd, &c, 1);
        if (r < 0) {
            if (errno == EINTR)
                continue;
            return -1;
        }
        if (r == 0)
            break;
        if (c == '\n')
            break;
        if (c != '\r')
            buf[i++] = c;
    }
    buf[i] = 0;
    return (int)i;
}

int ipc_request(const char *name, const char *req, char *resp, size_t resp_n)
{
    if (resp && resp_n)
        resp[0] = 0;
    int fd = ipc_connect(name);
    if (fd < 0) {
        if (resp && resp_n)
            str_copy(resp, resp_n, "ERR offline");
        return -1;
    }
    if (ipc_send_line(fd, req ? req : "STATUS") != 0) {
        close(fd);
        if (resp && resp_n)
            str_copy(resp, resp_n, "ERR send");
        return -1;
    }
    char line[VILAOS_IPC_MAX_MSG];
    if (ipc_recv_line(fd, line, sizeof(line)) < 0) {
        close(fd);
        if (resp && resp_n)
            str_copy(resp, resp_n, "ERR recv");
        return -1;
    }
    close(fd);
    if (resp && resp_n)
        str_copy(resp, resp_n, line);
    return 0;
}

void ipc_close(int fd)
{
    if (fd >= 0)
        close(fd);
}
#endif

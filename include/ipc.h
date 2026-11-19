#ifndef VILAOS_IPC_H
#define VILAOS_IPC_H

#include <stddef.h>

#define VILAOS_RUN_DIR "/run/vilaos"
#define VILAOS_IPC_MAX_MSG 512

int ipc_ensure_rundir(void);
int ipc_sock_path(const char *name, char *out, size_t n);
int ipc_listen(const char *name);
int ipc_accept(int listen_fd);
int ipc_connect(const char *name);
int ipc_send_line(int fd, const char *line);
int ipc_recv_line(int fd, char *buf, size_t n);
int ipc_request(const char *name, const char *req, char *resp, size_t resp_n);
void ipc_close(int fd);

#endif

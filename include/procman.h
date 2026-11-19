#ifndef VILAOS_PROCMAN_H
#define VILAOS_PROCMAN_H

#include "types.h"
#include "proctier.h"

#define PROCMAN_MAX_SVC 16
#define PROCMAN_ID_LEN 32

typedef enum {
    SVC_STOPPED = 0,
    SVC_STARTING,
    SVC_RUNNING,
    SVC_STOPPING
} SvcState;

typedef struct {
    char id[PROCMAN_ID_LEN];
    char binary[MAX_PATH];
    char sock[64];
    char args[128];
    int pid;
    SvcState state;
    int autostart;
    int declared;
} ProcService;

typedef struct {
    ProcService svcs[PROCMAN_MAX_SVC];
    int count;
    ProcTier tier;
    int ready;
} ProcMan;

int procman_init(ProcMan *pm);
void procman_shutdown(ProcMan *pm);
int procman_register(ProcMan *pm, const char *id, const char *binary, const char *sock, int autostart);
int procman_start(ProcMan *pm, const char *id);
int procman_stop(ProcMan *pm, const char *id);
void procman_stop_all(ProcMan *pm);
void procman_tick(ProcMan *pm);
int procman_status(ProcMan *pm, const char *id, char *out, size_t n);
int procman_cmd(ProcMan *pm, const char *id, const char *cmd, char *resp, size_t n);
ProcService *procman_find(ProcMan *pm, const char *id);
const ProcTier *procman_tier(const ProcMan *pm);

extern ProcMan g_procman;

#endif

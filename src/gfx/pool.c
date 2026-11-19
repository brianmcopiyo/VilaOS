#include "pool.h"
#include "util.h"

#include <stdlib.h>
#include <string.h>

#ifndef _WIN32
#include <pthread.h>
#endif

typedef struct {
    PoolJobFn fn;
    void *user;
    int y0, y1;
} BandJob;

#ifndef _WIN32
typedef struct {
    BandJob job;
    pthread_t thread;
} WorkerCtx;

static void *worker_main(void *arg)
{
    WorkerCtx *w = (WorkerCtx *)arg;
    if (w && w->job.fn)
        w->job.fn(w->job.y0, w->job.y1, w->job.user);
    return NULL;
}
#endif

int gfx_pool_init(GfxPool *p, const HwProfile *hw)
{
    if (!p)
        return -1;
    memset(p, 0, sizeof(*p));
    p->workers = hw && hw->workers > 0 ? hw->workers : 1;
    p->ready = 1;
    return 0;
}

void gfx_pool_shutdown(GfxPool *p)
{
    if (!p)
        return;
    p->ready = 0;
    p->workers = 0;
}

void gfx_pool_parallel_bands(GfxPool *p, int y0, int y1, PoolJobFn fn, void *user)
{
    if (!fn || y1 <= y0)
        return;
    int workers = (p && p->ready && p->workers > 0) ? p->workers : 1;
    if (workers < 1)
        workers = 1;
    int height = y1 - y0;
    if (workers == 1 || height < workers * 8) {
        fn(y0, y1, user);
        return;
    }
#ifndef _WIN32
    WorkerCtx *ctx = (WorkerCtx *)calloc((size_t)workers, sizeof(WorkerCtx));
    if (!ctx) {
        fn(y0, y1, user);
        return;
    }
    int band = (height + workers - 1) / workers;
    int launched = 0;
    for (int i = 0; i < workers; i++) {
        int by0 = y0 + i * band;
        int by1 = by0 + band;
        if (by0 >= y1)
            break;
        if (by1 > y1)
            by1 = y1;
        ctx[i].job.fn = fn;
        ctx[i].job.user = user;
        ctx[i].job.y0 = by0;
        ctx[i].job.y1 = by1;
        if (pthread_create(&ctx[i].thread, NULL, worker_main, &ctx[i]) != 0) {
            fn(by0, by1, user);
        } else {
            launched++;
        }
    }
    for (int i = 0; i < launched; i++)
        pthread_join(ctx[i].thread, NULL);
    free(ctx);
#else
    (void)workers;
    fn(y0, y1, user);
#endif
}

#ifndef VILAOS_VAPI_H
#define VILAOS_VAPI_H

#include "types.h"
#include "pkg.h"
#include "vsl.h"
#include "screen.h"
#include "fb.h"

#define VILAOS_API_VERSION 1

#define VAPI_OK                0
#define VAPI_ERR_PERMISSION   -1
#define VAPI_ERR_INVALID_ARG  -2
#define VAPI_ERR_NOT_FOUND    -3
#define VAPI_ERR_LIMIT        -4
#define VAPI_ERR_IO           -5
#define VAPI_ERR_UNSUPPORTED  -6
#define VAPI_ERR_INTERNAL     -7

typedef struct {
    size_t arg_max;
    size_t path_max;
    size_t buf_max;
    int queue_depth;
    int workers;
} VapiLimits;

typedef struct {
    VslRuntime *rt;
    Screen *screen;
    Gfx *gfx;
    const PkgApp *app;
    VapiLimits limits;
} VapiCtx;

void vapi_init(void);
const VapiLimits *vapi_limits(void);
unsigned vapi_cap_required(const char *method);
int vapi_check_cap(const VapiCtx *ctx, unsigned cap);
const char *vapi_status_name(int st);
int vapi_call(VapiCtx *ctx, const char *method, const char *a1, const char *a2, char *out, size_t out_n);
int vapi_bridge(VslRuntime *rt, const char *method, const char *a1, const char *a2);
int vapi_bridge_out(VslRuntime *rt, const char *method, const char *a1, const char *a2, char *out, size_t n);

int vapi_ui(VapiCtx *ctx, const char *op, const char *a1, const char *a2, char *out, size_t out_n);
int vapi_storage(VapiCtx *ctx, const char *op, const char *a1, const char *a2, char *out, size_t out_n);
int vapi_service(VapiCtx *ctx, const char *op, const char *a1, const char *a2, char *out, size_t out_n);
int vapi_audio(VapiCtx *ctx, const char *op, const char *a1, const char *a2, char *out, size_t out_n);
int vapi_telephony(VapiCtx *ctx, const char *op, const char *a1, const char *a2, char *out, size_t out_n);
int vapi_messages(VapiCtx *ctx, const char *op, const char *a1, const char *a2, char *out, size_t out_n);
int vapi_contacts(VapiCtx *ctx, const char *op, const char *a1, const char *a2, char *out, size_t out_n);
int vapi_clock(VapiCtx *ctx, const char *op, const char *a1, const char *a2, char *out, size_t out_n);
int vapi_system(VapiCtx *ctx, const char *op, const char *a1, const char *a2, char *out, size_t out_n);
int vapi_window(VapiCtx *ctx, const char *op, const char *a1, const char *a2, char *out, size_t out_n);
int vapi_pkg(VapiCtx *ctx, const char *op, const char *a1, const char *a2, char *out, size_t out_n);

#endif

#ifndef VILAOS_VSL_H
#define VILAOS_VSL_H

#include "types.h"
#include "pkg.h"

#define VSL_MAX_SRC 32768
#define VSL_MAX_VARS 64
#define VSL_MAX_HANDLERS 96
#define VSL_MAX_LINES 1024
#define VSL_MAX_FUNS 32
#define VSL_MAX_LISTS 8
#define VSL_MAX_LIST_ITEMS 32
#define VSL_MAX_STEPS 50000
#define VSL_MAX_CALL_DEPTH 8
#define VSL_MAX_PARAMS 6

typedef struct {
    char name[MAX_ID];
    char value[MAX_TEXT];
} VslVar;

typedef struct {
    char event[24];
    char target[MAX_ID];
    int line_start;
    int line_end;
} VslHandler;

typedef struct {
    char name[MAX_ID];
    char params[VSL_MAX_PARAMS][MAX_ID];
    int param_count;
    int line_start;
    int line_end;
} VslFun;

typedef struct {
    char items[VSL_MAX_LIST_ITEMS][MAX_TEXT];
    int count;
    int used;
} VslList;

typedef struct {
    char src[VSL_MAX_SRC];
    char *lines[VSL_MAX_LINES];
    int jump[VSL_MAX_LINES];
    int line_count;
    VslHandler handlers[VSL_MAX_HANDLERS];
    int handler_count;
    VslFun funs[VSL_MAX_FUNS];
    int fun_count;
    VslList lists[VSL_MAX_LISTS];
    VslVar vars[VSL_MAX_VARS];
    int var_count;
    char ret_val[MAX_TEXT];
    int returning;
    int steps;
    int depth;
    PkgApp app;
    Screen *screen;
    char nav[MAX_PATH];
    int loaded;
} VslRuntime;

int vsl_load(VslRuntime *rt, const PkgApp *app, Screen *screen, const char *script_path);
void vsl_reset(VslRuntime *rt);
int vsl_fire(VslRuntime *rt, const char *event, const char *target);
int vsl_has_nav(const VslRuntime *rt);
const char *vsl_take_nav(VslRuntime *rt);
int vsl_consume_nav(VslRuntime *rt, char *out, size_t out_sz);

#endif

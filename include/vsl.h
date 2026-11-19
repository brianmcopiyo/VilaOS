#ifndef VILAOS_VSL_H
#define VILAOS_VSL_H

#include "types.h"
#include "pkg.h"

#define VSL_MAX_SRC 8192
#define VSL_MAX_VARS 32
#define VSL_MAX_HANDLERS 48
#define VSL_MAX_LINES 256

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
    char src[VSL_MAX_SRC];
    char *lines[VSL_MAX_LINES];
    int line_count;
    VslHandler handlers[VSL_MAX_HANDLERS];
    int handler_count;
    VslVar vars[VSL_MAX_VARS];
    int var_count;
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

#endif

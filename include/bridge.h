#ifndef VILAOS_BRIDGE_H
#define VILAOS_BRIDGE_H

#include "vsl.h"
#include "screen.h"

int bridge_check_cap(const VslRuntime *rt, unsigned cap);
int bridge_set_text(VslRuntime *rt, const char *id, const char *text);
int bridge_get_text(VslRuntime *rt, const char *id, char *out, size_t n);
int bridge_append_text(VslRuntime *rt, const char *id, const char *text);
int bridge_clear_text(VslRuntime *rt, const char *id);
int bridge_storage_set(VslRuntime *rt, const char *key, const char *val);
int bridge_storage_get(VslRuntime *rt, const char *key, char *out, size_t n);
int bridge_nav(VslRuntime *rt, const char *screen_rel);
int bridge_call_out(VslRuntime *rt, const char *method, const char *a1, const char *a2, char *out, size_t n);
int bridge_exec_cmd(VslRuntime *rt, const char *cmd, const char *a1, const char *a2);

#endif

#ifndef PHONE_T9_H
#define PHONE_T9_H

#include "types.h"

void t9_reset(T9State *t);
char t9_feed(T9State *t, int digit, uint64_t now, int *replace);
char t9_symbol(T9State *t, uint64_t now, int *replace);
void t9_toggle_case(T9State *t);
int t9_pending(T9State *t, uint64_t now);
char t9_commit(T9State *t);

#endif

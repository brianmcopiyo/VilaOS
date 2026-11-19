#include "types.h"
#include "util.h"

#include <string.h>

static const char *t9_map[10] = {
    " 0",
    ".,?!1",
    "abc2",
    "def3",
    "ghi4",
    "jkl5",
    "mno6",
    "pqrs7",
    "tuv8",
    "wxyz9"
};

static const char *t9_sym = ".,?!@#$*()-_+/";

void t9_reset(T9State *t)
{
    memset(t, 0, sizeof(*t));
}

char t9_feed(T9State *t, int digit, uint64_t now, int *replace)
{
    *replace = 0;
    if (digit < 0 || digit > 9)
        return 0;

    if (t->len > 0 && t->key == digit && (now - t->last_ms) < 800) {
        const char *map = t9_map[digit];
        int n = (int)strlen(map);
        int idx = 0;
        for (int i = 0; i < n; i++) {
            char c = map[i];
            if (t->upper && c >= 'a' && c <= 'z')
                c = (char)(c - 'a' + 'A');
            if (c == t->buf[0]) {
                idx = (i + 1) % n;
                break;
            }
        }
        char c = map[idx];
        if (t->upper && c >= 'a' && c <= 'z')
            c = (char)(c - 'a' + 'A');
        t->buf[0] = c;
        t->buf[1] = 0;
        t->len = 1;
        t->last_ms = now;
        *replace = 1;
        return c;
    }

    const char *map = t9_map[digit];
    char c = map[0];
    if (t->upper && c >= 'a' && c <= 'z')
        c = (char)(c - 'a' + 'A');
    t->buf[0] = c;
    t->buf[1] = 0;
    t->len = 1;
    t->key = digit;
    t->last_ms = now;
    *replace = 0;
    return c;
}

char t9_symbol(T9State *t, uint64_t now, int *replace)
{
    *replace = 0;
    int n = (int)strlen(t9_sym);
    if (t->len > 0 && t->key == -1 && (now - t->last_ms) < 800) {
        int idx = 0;
        for (int i = 0; i < n; i++) {
            if (t9_sym[i] == t->buf[0]) {
                idx = (i + 1) % n;
                break;
            }
        }
        t->buf[0] = t9_sym[idx];
        t->buf[1] = 0;
        t->len = 1;
        t->last_ms = now;
        *replace = 1;
        return t->buf[0];
    }
    t->buf[0] = t9_sym[0];
    t->buf[1] = 0;
    t->len = 1;
    t->key = -1;
    t->last_ms = now;
    *replace = 0;
    return t->buf[0];
}

void t9_toggle_case(T9State *t)
{
    t->upper = !t->upper;
    if (t->len == 1) {
        char c = t->buf[0];
        if (c >= 'a' && c <= 'z')
            t->buf[0] = (char)(c - 'a' + 'A');
        else if (c >= 'A' && c <= 'Z')
            t->buf[0] = (char)(c - 'A' + 'a');
    }
}

int t9_pending(T9State *t, uint64_t now)
{
    return t->len > 0 && (now - t->last_ms) < 800;
}

char t9_commit(T9State *t)
{
    if (t->len <= 0)
        return 0;
    char c = t->buf[0];
    t->len = 0;
    t->buf[0] = 0;
    return c;
}

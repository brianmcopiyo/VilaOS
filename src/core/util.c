#include "util.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ctype.h>

#ifdef _WIN32
#include <windows.h>
#endif

uint64_t now_ms(void)
{
#ifdef _WIN32
    return (uint64_t)GetTickCount64();
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000ull + (uint64_t)ts.tv_nsec / 1000000ull;
#endif
}

void str_copy(char *dst, size_t dst_sz, const char *src)
{
    if (!dst || dst_sz == 0)
        return;
    if (!src) {
        dst[0] = 0;
        return;
    }
    size_t i = 0;
    while (src[i] && i + 1 < dst_sz) {
        dst[i] = src[i];
        i++;
    }
    dst[i] = 0;
}

void str_ncopy(char *dst, size_t dst_sz, const char *src, size_t n)
{
    if (!dst || dst_sz == 0)
        return;
    if (!src) {
        dst[0] = 0;
        return;
    }
    size_t i = 0;
    while (src[i] && i < n && i + 1 < dst_sz) {
        dst[i] = src[i];
        i++;
    }
    dst[i] = 0;
}

int str_eq(const char *a, const char *b)
{
    if (!a || !b)
        return a == b;
    return strcmp(a, b) == 0;
}

int str_starts(const char *s, const char *pfx)
{
    if (!s || !pfx)
        return 0;
    while (*pfx) {
        if (*s++ != *pfx++)
            return 0;
    }
    return 1;
}

size_t str_len(const char *s)
{
    return s ? strlen(s) : 0;
}

void str_cat(char *dst, size_t dst_sz, const char *src)
{
    if (!dst || !src || dst_sz == 0)
        return;
    size_t n = strlen(dst);
    str_copy(dst + n, dst_sz - n, src);
}

static int hex_nibble(char c)
{
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;
    return -1;
}

int parse_hex_color(const char *s, uint16_t *out)
{
    if (!s || !out)
        return -1;
    if (s[0] == '#')
        s++;
    int r, g, b;
    if (strlen(s) == 6) {
        int v[6];
        for (int i = 0; i < 6; i++) {
            v[i] = hex_nibble(s[i]);
            if (v[i] < 0)
                return -1;
        }
        r = (v[0] << 4) | v[1];
        g = (v[2] << 4) | v[3];
        b = (v[4] << 4) | v[5];
        *out = RGB565(r, g, b);
        return 0;
    }
    if (strlen(s) == 3) {
        int v[3];
        for (int i = 0; i < 3; i++) {
            v[i] = hex_nibble(s[i]);
            if (v[i] < 0)
                return -1;
        }
        r = v[0] * 17;
        g = v[1] * 17;
        b = v[2] * 17;
        *out = RGB565(r, g, b);
        return 0;
    }
    return -1;
}

int parse_dim(const char *s, int *out_val, int *out_unit)
{
    if (!out_val || !out_unit)
        return -1;
    *out_val = 0;
    *out_unit = UNIT_AUTO;
    if (!s || !s[0])
        return 0;
    if (str_eq(s, "auto") || str_eq(s, "fill") || str_eq(s, "*")) {
        *out_unit = UNIT_AUTO;
        return 0;
    }
    char *end = NULL;
    long v = strtol(s, &end, 10);
    if (end == s)
        return -1;
    while (*end && isspace((unsigned char)*end))
        end++;
    if (*end == '%') {
        *out_unit = UNIT_PCT;
        *out_val = (int)v;
        return 0;
    }
    if (*end != 0)
        return -1;
    *out_unit = UNIT_PX;
    *out_val = (int)v;
    return 0;
}

int clamp_i(int v, int lo, int hi)
{
    if (v < lo)
        return lo;
    if (v > hi)
        return hi;
    return v;
}

int scale_i(int v, int num, int den)
{
    if (den <= 0)
        return v;
    return (int)(((long)v * (long)num) / (long)den);
}

void rect_intersect(Rect *a, const Rect *b)
{
    int x0 = a->x > b->x ? a->x : b->x;
    int y0 = a->y > b->y ? a->y : b->y;
    int x1 = (a->x + a->w) < (b->x + b->w) ? (a->x + a->w) : (b->x + b->w);
    int y1 = (a->y + a->h) < (b->y + b->h) ? (a->y + a->h) : (b->y + b->h);
    a->x = x0;
    a->y = y0;
    a->w = x1 > x0 ? x1 - x0 : 0;
    a->h = y1 > y0 ? y1 - y0 : 0;
}

int rect_contains(const Rect *r, int x, int y)
{
    return x >= r->x && y >= r->y && x < r->x + r->w && y < r->y + r->h;
}

uint64_t read_uptime_ms(void)
{
    FILE *f = fopen("/proc/uptime", "r");
    if (!f)
        return 0;
    double up = 0;
    if (fscanf(f, "%lf", &up) != 1)
        up = 0;
    fclose(f);
    return (uint64_t)(up * 1000.0);
}

int read_meminfo_kb(long *total_kb, long *free_kb, long *avail_kb)
{
    FILE *f = fopen("/proc/meminfo", "r");
    if (!f)
        return -1;
    char line[128];
    long total = 0, freeb = 0, avail = 0;
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "MemTotal:", 9) == 0)
            sscanf(line + 9, "%ld", &total);
        else if (strncmp(line, "MemFree:", 8) == 0)
            sscanf(line + 8, "%ld", &freeb);
        else if (strncmp(line, "MemAvailable:", 13) == 0)
            sscanf(line + 13, "%ld", &avail);
    }
    fclose(f);
    if (total_kb)
        *total_kb = total;
    if (free_kb)
        *free_kb = freeb;
    if (avail_kb)
        *avail_kb = avail;
    return 0;
}

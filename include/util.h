#ifndef PHONE_UTIL_H
#define PHONE_UTIL_H

#include "types.h"

uint64_t now_ms(void);
void str_copy(char *dst, size_t dst_sz, const char *src);
void str_ncopy(char *dst, size_t dst_sz, const char *src, size_t n);
int str_eq(const char *a, const char *b);
int str_starts(const char *s, const char *pfx);
size_t str_len(const char *s);
void str_cat(char *dst, size_t dst_sz, const char *src);
int parse_hex_color(const char *s, uint16_t *out);
int parse_dim(const char *s, int *out_val, int *out_unit);
int clamp_i(int v, int lo, int hi);
int scale_i(int v, int num, int den);
void rect_intersect(Rect *a, const Rect *b);
int rect_contains(const Rect *r, int x, int y);
uint64_t read_uptime_ms(void);
int read_meminfo_kb(long *total_kb, long *free_kb, long *avail_kb);

#endif

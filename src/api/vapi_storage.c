#include "vapi.h"
#include "bridge.h"
#include "util.h"

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#ifdef _WIN32
#include <direct.h>
#define mkdir_mode(p, m) _mkdir(p)
#else
#include <dirent.h>
#include <unistd.h>
#define mkdir_mode(p, m) mkdir(p, m)
#endif

static int valid_key(const char *key, size_t max_len)
{
    if (!key || !key[0] || strlen(key) > max_len)
        return 0;
    if (strstr(key, "..") || strchr(key, '/') || strchr(key, '\\') || strchr(key, ':'))
        return 0;
    for (const char *p = key; *p; p++) {
        unsigned char c = (unsigned char)*p;
        if (!(c == '_' || c == '-' || c == '.' || (c >= 'a' && c <= 'z') ||
              (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')))
            return 0;
    }
    return 1;
}

static int valid_rel_path(const char *rel, size_t max_len)
{
    if (!rel || !rel[0] || strlen(rel) > max_len)
        return 0;
    if (rel[0] == '/' || rel[0] == '\\')
        return 0;
    if (strstr(rel, ".."))
        return 0;
    return 1;
}

static int ensure_app_data(const VapiCtx *ctx)
{
    char path[MAX_PATH];
    if (!ctx || !ctx->app)
        return -1;
    pkg_data_path(ctx->app->id, path, sizeof(path));
    mkdir_mode(PKG_DATA_ROOT, 0755);
    mkdir_mode(path, 0755);
    return 0;
}

static int storage_app_set(VapiCtx *ctx, const char *key, const char *val)
{
    if (!valid_key(key, 64))
        return VAPI_ERR_INVALID_ARG;
    if (ensure_app_data(ctx) != 0)
        return VAPI_ERR_IO;
    char path[MAX_PATH];
    snprintf(path, sizeof(path), "%s/%s/%s", PKG_DATA_ROOT, ctx->app->id, key);
    if (strlen(path) >= ctx->limits.path_max)
        return VAPI_ERR_LIMIT;
    FILE *f = fopen(path, "w");
    if (!f)
        return VAPI_ERR_IO;
    if (val) {
        size_t n = strlen(val);
        if (n > ctx->limits.buf_max) {
            fclose(f);
            return VAPI_ERR_LIMIT;
        }
        fputs(val, f);
    }
    fclose(f);
    return VAPI_OK;
}

static int storage_app_get(VapiCtx *ctx, const char *key, char *out, size_t out_n)
{
    if (!valid_key(key, 64) || !out || out_n == 0)
        return VAPI_ERR_INVALID_ARG;
    out[0] = 0;
    char path[MAX_PATH];
    snprintf(path, sizeof(path), "%s/%s/%s", PKG_DATA_ROOT, ctx->app->id, key);
    FILE *f = fopen(path, "r");
    if (!f)
        return VAPI_ERR_NOT_FOUND;
    size_t cap = out_n < ctx->limits.buf_max ? out_n : ctx->limits.buf_max;
    if (!fgets(out, (int)cap, f))
        out[0] = 0;
    fclose(f);
    size_t L = strlen(out);
    while (L > 0 && (out[L - 1] == '\n' || out[L - 1] == '\r'))
        out[--L] = 0;
    return VAPI_OK;
}

static int storage_asset_read(VapiCtx *ctx, const char *rel, char *out, size_t out_n)
{
    if (!valid_rel_path(rel, ctx->limits.path_max) || !out || out_n == 0)
        return VAPI_ERR_INVALID_ARG;
    out[0] = 0;
    char path[MAX_PATH];
    snprintf(path, sizeof(path), "%s/%s/res/assets/%s", PKG_APPS_ROOT, ctx->app->id, rel);
    FILE *f = fopen(path, "r");
    if (!f)
        return VAPI_ERR_NOT_FOUND;
    size_t cap = out_n < ctx->limits.buf_max ? out_n : ctx->limits.buf_max;
    size_t n = fread(out, 1, cap > 0 ? cap - 1 : 0, f);
    out[n] = 0;
    fclose(f);
    return VAPI_OK;
}

static int storage_files_list(VapiCtx *ctx, const char *rel, char *out, size_t out_n)
{
    if (!out || out_n == 0)
        return VAPI_ERR_INVALID_ARG;
    out[0] = 0;
    if (ensure_app_data(ctx) != 0)
        return VAPI_ERR_IO;
    char path[MAX_PATH];
    if (!rel || !rel[0] || str_eq(rel, "."))
        snprintf(path, sizeof(path), "%s/%s", PKG_DATA_ROOT, ctx->app->id);
    else {
        if (!valid_rel_path(rel, ctx->limits.path_max))
            return VAPI_ERR_INVALID_ARG;
        snprintf(path, sizeof(path), "%s/%s/%s", PKG_DATA_ROOT, ctx->app->id, rel);
    }
#ifndef _WIN32
    DIR *d = opendir(path);
    if (!d)
        return VAPI_ERR_NOT_FOUND;
    struct dirent *de;
    size_t used = 0;
    int count = 0;
    while ((de = readdir(d)) != NULL) {
        if (de->d_name[0] == '.')
            continue;
        if (count >= ctx->limits.queue_depth)
            break;
        size_t need = strlen(de->d_name) + (used ? 2 : 1);
        if (used + need >= out_n)
            break;
        if (used)
            out[used++] = '\n';
        memcpy(out + used, de->d_name, strlen(de->d_name));
        used += strlen(de->d_name);
        out[used] = 0;
        count++;
    }
    closedir(d);
    return VAPI_OK;
#else
    (void)path;
    return VAPI_ERR_IO;
#endif
}

static int storage_files_read(VapiCtx *ctx, const char *rel, char *out, size_t out_n)
{
    if (!valid_rel_path(rel, ctx->limits.path_max) || !out || out_n == 0)
        return VAPI_ERR_INVALID_ARG;
    out[0] = 0;
    char path[MAX_PATH];
    snprintf(path, sizeof(path), "%s/%s/%s", PKG_DATA_ROOT, ctx->app->id, rel);
    FILE *f = fopen(path, "r");
    if (!f)
        return VAPI_ERR_NOT_FOUND;
    size_t cap = out_n < ctx->limits.buf_max ? out_n : ctx->limits.buf_max;
    size_t n = fread(out, 1, cap > 0 ? cap - 1 : 0, f);
    out[n] = 0;
    fclose(f);
    return VAPI_OK;
}

int vapi_storage(VapiCtx *ctx, const char *op, const char *a1, const char *a2, char *out, size_t out_n)
{
    if (!ctx || !op)
        return VAPI_ERR_INVALID_ARG;

    if (str_eq(op, "app.get") || str_eq(op, "get")) {
        char buf[MAX_TEXT];
        int st = storage_app_get(ctx, a1, buf, sizeof(buf));
        if (st != VAPI_OK)
            return st;
        if (a2 && a2[0] && ctx->rt) {
            if (bridge_set_text(ctx->rt, a2, buf) != 0)
                return VAPI_ERR_NOT_FOUND;
        } else if (out && out_n) {
            str_copy(out, out_n, buf);
        }
        return VAPI_OK;
    }
    if (str_eq(op, "app.set") || str_eq(op, "set"))
        return storage_app_set(ctx, a1, a2);
    if (str_eq(op, "asset.read") || str_eq(op, "asset")) {
        char buf[MAX_TEXT];
        int st = storage_asset_read(ctx, a1, buf, sizeof(buf));
        if (st != VAPI_OK)
            return st;
        if (a2 && a2[0] && ctx->rt) {
            if (bridge_set_text(ctx->rt, a2, buf) != 0)
                return VAPI_ERR_NOT_FOUND;
        } else if (out && out_n) {
            str_copy(out, out_n, buf);
        }
        return VAPI_OK;
    }
    if (str_eq(op, "files.list") || str_eq(op, "list")) {
        char buf[MAX_TEXT];
        int st = storage_files_list(ctx, a1, buf, sizeof(buf));
        if (st != VAPI_OK)
            return st;
        if (a2 && a2[0] && ctx->rt)
            bridge_set_text(ctx->rt, a2, buf);
        if (out && out_n)
            str_copy(out, out_n, buf);
        return VAPI_OK;
    }
    if (str_eq(op, "files.read") || str_eq(op, "read")) {
        char buf[MAX_TEXT];
        int st = storage_files_read(ctx, a1, buf, sizeof(buf));
        if (st != VAPI_OK)
            return st;
        if (a2 && a2[0] && ctx->rt)
            bridge_set_text(ctx->rt, a2, buf);
        if (out && out_n)
            str_copy(out, out_n, buf);
        return VAPI_OK;
    }
    return VAPI_ERR_UNSUPPORTED;
}

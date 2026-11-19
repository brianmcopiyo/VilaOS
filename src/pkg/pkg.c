#include "pkg.h"
#include "util.h"
#include "xml.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#ifdef _WIN32
#include <direct.h>
#define mkdir_mode(p, m) _mkdir(p)
#else
#include <unistd.h>
#include <dirent.h>
#define mkdir_mode(p, m) mkdir(p, m)
#endif

unsigned pkg_cap_from_name(const char *name)
{
    if (!name)
        return 0;
    if (str_eq(name, "telephony"))
        return PKG_CAP_TELEPHONY;
    if (str_eq(name, "messages"))
        return PKG_CAP_MESSAGES;
    if (str_eq(name, "contacts"))
        return PKG_CAP_CONTACTS;
    if (str_eq(name, "storage"))
        return PKG_CAP_STORAGE;
    if (str_eq(name, "clock"))
        return PKG_CAP_CLOCK;
    if (str_eq(name, "display"))
        return PKG_CAP_DISPLAY;
    if (str_eq(name, "network"))
        return PKG_CAP_NETWORK;
    return 0;
}

typedef struct {
    PkgApp *app;
} ManCtx;

static void man_start(const XmlStart *el, void *user)
{
    ManCtx *ctx = (ManCtx *)user;
    PkgApp *a = ctx->app;
    if (str_eq(el->tag, "app")) {
        const char *id = xml_attr(el, "id");
        const char *name = xml_attr(el, "name");
        const char *ver = xml_attr(el, "version");
        const char *vc = xml_attr(el, "version_code");
        const char *icon = xml_attr(el, "icon");
        if (id)
            str_copy(a->id, sizeof(a->id), id);
        if (name)
            str_copy(a->name, sizeof(a->name), name);
        if (ver)
            str_copy(a->version, sizeof(a->version), ver);
        if (vc)
            a->version_code = atoi(vc);
        if (icon)
            str_copy(a->icon, sizeof(a->icon), icon);
    } else if (str_eq(el->tag, "entry")) {
        const char *screen = xml_attr(el, "screen");
        const char *script = xml_attr(el, "script");
        if (screen)
            str_copy(a->entry_screen, sizeof(a->entry_screen), screen);
        if (script)
            str_copy(a->entry_script, sizeof(a->entry_script), script);
    } else if (str_eq(el->tag, "capability")) {
        const char *n = xml_attr(el, "name");
        a->caps |= pkg_cap_from_name(n);
    } else if (str_eq(el->tag, "summary")) {
        const char *t = xml_attr(el, "text");
        if (t)
            str_copy(a->summary, sizeof(a->summary), t);
    }
}

static void man_text(const char *text, void *user)
{
    ManCtx *ctx = (ManCtx *)user;
    if (ctx->app->summary[0] == 0 && text && text[0])
        str_copy(ctx->app->summary, sizeof(ctx->app->summary), text);
}

int pkg_parse_manifest(const char *path, PkgApp *out)
{
    if (!path || !out)
        return -1;
    memset(out, 0, sizeof(*out));
    str_copy(out->entry_screen, sizeof(out->entry_screen), "ui/main.xml");
    str_copy(out->entry_script, sizeof(out->entry_script), "logic/main.vsl");
    str_copy(out->version, sizeof(out->version), "0.0.0");
    ManCtx ctx = {out};
    XmlHandlers h;
    memset(&h, 0, sizeof(h));
    h.on_start = man_start;
    h.on_text = man_text;
    h.user = &ctx;
    if (xml_parse_file(path, &h) != 0)
        return -1;
    if (!out->id[0])
        return -1;
    out->installed = 1;
    return 0;
}

int pkg_app_path(const char *id, char *out, size_t n)
{
    if (!id || !out || n < 8)
        return -1;
    snprintf(out, n, "%s/%s", PKG_APPS_ROOT, id);
    return 0;
}

int pkg_data_path(const char *id, char *out, size_t n)
{
    if (!id || !out || n < 8)
        return -1;
    snprintf(out, n, "%s/%s", PKG_DATA_ROOT, id);
    return 0;
}

static int ensure_dir(const char *path)
{
    char tmp[MAX_PATH];
    str_copy(tmp, sizeof(tmp), path);
    size_t len = strlen(tmp);
    if (len == 0)
        return -1;
    for (size_t i = 1; i < len; i++) {
        if (tmp[i] == '/' || tmp[i] == '\\') {
            char c = tmp[i];
            tmp[i] = 0;
            mkdir_mode(tmp, 0755);
            tmp[i] = c;
        }
    }
    mkdir_mode(tmp, 0755);
    return 0;
}

static int copy_file(const char *src, const char *dst)
{
    FILE *in = fopen(src, "rb");
    if (!in)
        return -1;
    ensure_dir(dst);
    {
        char parent[MAX_PATH];
        str_copy(parent, sizeof(parent), dst);
        char *slash = strrchr(parent, '/');
#ifdef _WIN32
        char *b = strrchr(parent, '\\');
        if (b && (!slash || b > slash))
            slash = b;
#endif
        if (slash) {
            *slash = 0;
            ensure_dir(parent);
        }
    }
    FILE *out = fopen(dst, "wb");
    if (!out) {
        fclose(in);
        return -1;
    }
    char buf[1024];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), in)) > 0)
        fwrite(buf, 1, n, out);
    fclose(in);
    fclose(out);
    return 0;
}

#ifndef _WIN32
static int copy_tree(const char *src, const char *dst)
{
    DIR *d = opendir(src);
    if (!d)
        return -1;
    ensure_dir(dst);
    struct dirent *de;
    while ((de = readdir(d)) != NULL) {
        if (str_eq(de->d_name, ".") || str_eq(de->d_name, ".."))
            continue;
        char sp[MAX_PATH], dp[MAX_PATH];
        snprintf(sp, sizeof(sp), "%s/%s", src, de->d_name);
        snprintf(dp, sizeof(dp), "%s/%s", dst, de->d_name);
        struct stat st;
        if (stat(sp, &st) != 0)
            continue;
        if (S_ISDIR(st.st_mode)) {
            if (copy_tree(sp, dp) != 0) {
                closedir(d);
                return -1;
            }
        } else {
            if (copy_file(sp, dp) != 0) {
                closedir(d);
                return -1;
            }
        }
    }
    closedir(d);
    return 0;
}

static int rm_tree(const char *path)
{
    DIR *d = opendir(path);
    if (!d)
        return 0;
    struct dirent *de;
    while ((de = readdir(d)) != NULL) {
        if (str_eq(de->d_name, ".") || str_eq(de->d_name, ".."))
            continue;
        char p[MAX_PATH];
        snprintf(p, sizeof(p), "%s/%s", path, de->d_name);
        struct stat st;
        if (stat(p, &st) != 0)
            continue;
        if (S_ISDIR(st.st_mode))
            rm_tree(p);
        else
            unlink(p);
    }
    closedir(d);
    rmdir(path);
    return 0;
}
#endif

int pkg_registry_load(PkgRegistry *reg)
{
    if (!reg)
        return -1;
    memset(reg, 0, sizeof(*reg));
#ifndef _WIN32
    DIR *d = opendir(PKG_APPS_ROOT);
    if (!d)
        return 0;
    struct dirent *de;
    while ((de = readdir(d)) != NULL) {
        if (de->d_name[0] == '.')
            continue;
        if (reg->count >= PKG_MAX_APPS)
            break;
        char man[MAX_PATH];
        snprintf(man, sizeof(man), "%s/%s/manifest.xml", PKG_APPS_ROOT, de->d_name);
        PkgApp app;
        if (pkg_parse_manifest(man, &app) == 0) {
            if (!app.id[0])
                str_copy(app.id, sizeof(app.id), de->d_name);
            reg->apps[reg->count++] = app;
        }
    }
    closedir(d);
#else
    (void)reg;
#endif
    return 0;
}

int pkg_registry_save(const PkgRegistry *reg)
{
    if (!reg)
        return -1;
    ensure_dir("/var/lib/vilaos");
    FILE *f = fopen(PKG_REG_PATH, "w");
    if (!f)
        return -1;
    for (int i = 0; i < reg->count; i++) {
        const PkgApp *a = &reg->apps[i];
        fprintf(f, "[%s]\nname=%s\nversion=%s\nversion_code=%d\nicon=%s\nentry=%s\nscript=%s\ncaps=%u\n\n",
                a->id, a->name, a->version, a->version_code, a->icon, a->entry_screen, a->entry_script, a->caps);
    }
    fclose(f);
    return 0;
}

int pkg_registry_find(const PkgRegistry *reg, const char *id)
{
    if (!reg || !id)
        return -1;
    for (int i = 0; i < reg->count; i++) {
        if (str_eq(reg->apps[i].id, id))
            return i;
    }
    return -1;
}

static int registry_upsert(PkgRegistry *reg, const PkgApp *app)
{
    int idx = pkg_registry_find(reg, app->id);
    if (idx >= 0)
        reg->apps[idx] = *app;
    else if (reg->count < PKG_MAX_APPS)
        reg->apps[reg->count++] = *app;
    else
        return -1;
    return pkg_registry_save(reg);
}

#ifndef _WIN32
static int extract_vpkg_tar(const char *vpkg, const char *dest)
{
    char cmd[MAX_PATH * 2];
    ensure_dir(dest);
    snprintf(cmd, sizeof(cmd), "tar -xf '%s' -C '%s' 2>/dev/null", vpkg, dest);
    int rc = system(cmd);
    if (rc != 0) {
        snprintf(cmd, sizeof(cmd), "busybox tar -xf '%s' -C '%s' 2>/dev/null", vpkg, dest);
        rc = system(cmd);
    }
    return rc == 0 ? 0 : -1;
}
#endif

int pkg_install(const char *vpkg_path, PkgRegistry *reg)
{
#ifndef _WIN32
    if (!vpkg_path || !reg)
        return -1;
    char tmp[MAX_PATH];
    snprintf(tmp, sizeof(tmp), "/tmp/vilaos-pkg-%u", (unsigned)getpid());
    rm_tree(tmp);
    ensure_dir(tmp);
    if (extract_vpkg_tar(vpkg_path, tmp) != 0) {
        if (copy_tree(vpkg_path, tmp) != 0) {
            rm_tree(tmp);
            return -1;
        }
    }
    char man[MAX_PATH];
    snprintf(man, sizeof(man), "%s/manifest.xml", tmp);
    PkgApp app;
    if (pkg_parse_manifest(man, &app) != 0) {
        rm_tree(tmp);
        return -1;
    }
    char dest[MAX_PATH], data[MAX_PATH];
    pkg_app_path(app.id, dest, sizeof(dest));
    pkg_data_path(app.id, data, sizeof(data));
    ensure_dir(data);
    rm_tree(dest);
    if (copy_tree(tmp, dest) != 0) {
        rm_tree(tmp);
        return -1;
    }
    rm_tree(tmp);
    app.installed = 1;
    return registry_upsert(reg, &app);
#else
    (void)vpkg_path;
    (void)reg;
    return -1;
#endif
}

int pkg_update(const char *vpkg_path, PkgRegistry *reg)
{
    return pkg_install(vpkg_path, reg);
}

int pkg_remove(const char *id, PkgRegistry *reg)
{
#ifndef _WIN32
    if (!id || !reg)
        return -1;
    char dest[MAX_PATH];
    pkg_app_path(id, dest, sizeof(dest));
    rm_tree(dest);
    int idx = pkg_registry_find(reg, id);
    if (idx >= 0) {
        for (int i = idx; i < reg->count - 1; i++)
            reg->apps[i] = reg->apps[i + 1];
        reg->count--;
    }
    return pkg_registry_save(reg);
#else
    (void)id;
    (void)reg;
    return -1;
#endif
}

int pkg_list(const PkgRegistry *reg, char *out, size_t out_sz)
{
    if (!reg || !out || out_sz == 0)
        return -1;
    out[0] = 0;
    for (int i = 0; i < reg->count; i++) {
        char line[128];
        snprintf(line, sizeof(line), "%s %s (%s)\n", reg->apps[i].id, reg->apps[i].name, reg->apps[i].version);
        if (strlen(out) + strlen(line) + 1 < out_sz)
            str_cat(out, out_sz, line);
    }
    return reg->count;
}

int pkg_seed_from_dir(const char *apps_src_root, PkgRegistry *reg)
{
#ifndef _WIN32
    if (!apps_src_root || !reg)
        return -1;
    DIR *d = opendir(apps_src_root);
    if (!d)
        return -1;
    ensure_dir(PKG_APPS_ROOT);
    ensure_dir(PKG_DATA_ROOT);
    ensure_dir("/var/lib/vilaos");
    struct dirent *de;
    while ((de = readdir(d)) != NULL) {
        if (de->d_name[0] == '.')
            continue;
        char src[MAX_PATH], man[MAX_PATH], dst[MAX_PATH], data[MAX_PATH];
        snprintf(src, sizeof(src), "%s/%s", apps_src_root, de->d_name);
        snprintf(man, sizeof(man), "%s/manifest.xml", src);
        PkgApp app;
        if (pkg_parse_manifest(man, &app) != 0)
            continue;
        pkg_app_path(app.id, dst, sizeof(dst));
        pkg_data_path(app.id, data, sizeof(data));
        ensure_dir(data);
        rm_tree(dst);
        if (copy_tree(src, dst) != 0)
            continue;
        registry_upsert(reg, &app);
    }
    closedir(d);
    return 0;
#else
    (void)apps_src_root;
    (void)reg;
    return -1;
#endif
}

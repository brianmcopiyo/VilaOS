#include "pkg.h"
#include "util.h"
#include "xml.h"
#include "screen.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
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
    if (str_eq(name, "storage") || str_eq(name, "storage.app"))
        return PKG_CAP_STORAGE;
    if (str_eq(name, "storage.files"))
        return PKG_CAP_STORAGE_FILES;
    if (str_eq(name, "clock") || str_eq(name, "system.clock"))
        return PKG_CAP_CLOCK;
    if (str_eq(name, "display"))
        return PKG_CAP_DISPLAY;
    if (str_eq(name, "network"))
        return PKG_CAP_NETWORK;
    if (str_eq(name, "audio") || str_eq(name, "media") || str_eq(name, "audio.bg"))
        return PKG_CAP_AUDIO;
    if (str_eq(name, "ui"))
        return PKG_CAP_UI;
    if (str_eq(name, "system.power"))
        return PKG_CAP_SYSTEM_POWER;
    if (str_eq(name, "system.device"))
        return PKG_CAP_SYSTEM_DEVICE | PKG_CAP_DISPLAY;
    return 0;
}

typedef struct {
    PkgApp *app;
    int bad_cap;
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
        unsigned c = pkg_cap_from_name(n);
        if (!n || !n[0] || c == 0)
            ctx->bad_cap = 1;
        else
            a->caps |= c;
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
    str_copy(out->entry_screen, sizeof(out->entry_screen), "res/layout/main.xml");
    str_copy(out->entry_script, sizeof(out->entry_script), "bin/main.kbc");
    str_copy(out->version, sizeof(out->version), "0.0.0");
    ManCtx ctx = {out, 0};
    XmlHandlers h;
    memset(&h, 0, sizeof(h));
    h.on_start = man_start;
    h.on_text = man_text;
    h.user = &ctx;
    if (xml_parse_file(path, &h) != 0)
        return -1;
    if (!out->id[0] || ctx.bad_cap)
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
    if (registry_upsert(reg, &app) != 0)
        return -1;
    pkg_manager_notify_changed();
    return 0;
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
    if (pkg_registry_save(reg) != 0)
        return -1;
    pkg_manager_notify_changed();
    return 0;
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

#define PKG_MAX_LISTENERS 8

static PkgRegistry g_pkg_reg;
static int g_pkg_ready;
static int g_pkg_generation;
static int g_pkg_seen_generation;
static PkgChangedFn g_pkg_listeners[PKG_MAX_LISTENERS];
static void *g_pkg_listener_users[PKG_MAX_LISTENERS];
static int g_pkg_listener_count;

int pkg_manager_caps_text(unsigned caps, char *buf, size_t n)
{
    if (!buf || n == 0)
        return -1;
    buf[0] = 0;
    if (caps & PKG_CAP_UI)
        str_cat(buf, n, "ui ");
    if (caps & PKG_CAP_STORAGE)
        str_cat(buf, n, "storage.app ");
    if (caps & PKG_CAP_STORAGE_FILES)
        str_cat(buf, n, "storage.files ");
    if (caps & PKG_CAP_TELEPHONY)
        str_cat(buf, n, "telephony ");
    if (caps & PKG_CAP_MESSAGES)
        str_cat(buf, n, "messages ");
    if (caps & PKG_CAP_CONTACTS)
        str_cat(buf, n, "contacts ");
    if (caps & PKG_CAP_AUDIO)
        str_cat(buf, n, "audio.bg ");
    if (caps & PKG_CAP_CLOCK)
        str_cat(buf, n, "system.clock ");
    if (caps & PKG_CAP_SYSTEM_DEVICE)
        str_cat(buf, n, "system.device ");
    if (caps & PKG_CAP_SYSTEM_POWER)
        str_cat(buf, n, "system.power ");
    if (caps & PKG_CAP_NETWORK)
        str_cat(buf, n, "network ");
    if (caps & PKG_CAP_DISPLAY)
        str_cat(buf, n, "display ");
    if (!buf[0])
        str_copy(buf, n, "none");
    return 0;
}

void pkg_manager_notify_changed(void)
{
    if (g_pkg_ready)
        pkg_registry_load(&g_pkg_reg);
    g_pkg_generation++;
    for (int i = 0; i < g_pkg_listener_count; i++) {
        if (g_pkg_listeners[i])
            g_pkg_listeners[i](g_pkg_listener_users[i]);
    }
}

int pkg_manager_init(void)
{
    if (g_pkg_ready)
        return 0;
    memset(&g_pkg_reg, 0, sizeof(g_pkg_reg));
    g_pkg_listener_count = 0;
    g_pkg_generation = 0;
    g_pkg_seen_generation = 0;
    g_pkg_ready = 1;
    pkg_registry_load(&g_pkg_reg);
    if (g_pkg_reg.count == 0)
        pkg_seed_from_dir("/apps", &g_pkg_reg);
    g_pkg_generation = 1;
    return g_pkg_reg.count;
}

int pkg_manager_refresh(void)
{
    if (!g_pkg_ready)
        return pkg_manager_init();
    pkg_registry_load(&g_pkg_reg);
    if (g_pkg_reg.count == 0)
        pkg_seed_from_dir("/apps", &g_pkg_reg);
    return g_pkg_reg.count;
}

const PkgRegistry *pkg_manager_registry(void)
{
    if (!g_pkg_ready)
        pkg_manager_init();
    return &g_pkg_reg;
}

int pkg_manager_count(void)
{
    if (!g_pkg_ready)
        pkg_manager_init();
    return g_pkg_reg.count;
}

int pkg_manager_get(int index, PkgApp *out)
{
    if (!g_pkg_ready)
        pkg_manager_init();
    if (!out || index < 0 || index >= g_pkg_reg.count)
        return -1;
    *out = g_pkg_reg.apps[index];
    return 0;
}

int pkg_manager_find(const char *id, PkgApp *out)
{
    if (!g_pkg_ready)
        pkg_manager_init();
    int idx = pkg_registry_find(&g_pkg_reg, id);
    if (idx < 0)
        return -1;
    if (out)
        *out = g_pkg_reg.apps[idx];
    return 0;
}

int pkg_manager_list_json(char *out, size_t out_n)
{
    if (!out || out_n == 0)
        return -1;
    if (!g_pkg_ready)
        pkg_manager_init();
    out[0] = 0;
    str_cat(out, out_n, "[");
    for (int i = 0; i < g_pkg_reg.count; i++) {
        char caps[MAX_TEXT];
        char item[256];
        pkg_manager_caps_text(g_pkg_reg.apps[i].caps, caps, sizeof(caps));
        snprintf(item, sizeof(item), "%s%s:%s:%s:%s",
                 i ? ";" : "",
                 g_pkg_reg.apps[i].id,
                 g_pkg_reg.apps[i].name,
                 g_pkg_reg.apps[i].version,
                 g_pkg_reg.apps[i].icon[0] ? g_pkg_reg.apps[i].icon : g_pkg_reg.apps[i].id);
        if (strlen(out) + strlen(item) + 2 < out_n)
            str_cat(out, out_n, item);
    }
    str_cat(out, out_n, "]");
    return g_pkg_reg.count;
}

int pkg_manager_on_changed(PkgChangedFn fn, void *user)
{
    if (!fn || g_pkg_listener_count >= PKG_MAX_LISTENERS)
        return -1;
    g_pkg_listeners[g_pkg_listener_count] = fn;
    g_pkg_listener_users[g_pkg_listener_count] = user;
    g_pkg_listener_count++;
    return 0;
}

int pkg_manager_poll_changed(void)
{
    if (g_pkg_seen_generation == g_pkg_generation)
        return 0;
    g_pkg_seen_generation = g_pkg_generation;
    return 1;
}

static int pkg_str_has(const char *hay, const char *needle)
{
    char a[128];
    char b[64];
    size_t i;
    if (!needle || !needle[0])
        return 1;
    if (!hay)
        return 0;
    for (i = 0; hay[i] && i + 1 < sizeof(a); i++)
        a[i] = (char)tolower((unsigned char)hay[i]);
    a[i] = 0;
    for (i = 0; needle[i] && i + 1 < sizeof(b); i++)
        b[i] = (char)tolower((unsigned char)needle[i]);
    b[i] = 0;
    return strstr(a, b) != NULL;
}

static int pkg_alloc_button(Screen *s, Widget *grid, int grid_idx)
{
    if (!s || !grid || s->count >= MAX_WIDGETS || grid->child_count >= MAX_CHILDREN)
        return -1;
    int idx = s->count++;
    Widget *w = &s->widgets[idx];
    memset(w, 0, sizeof(*w));
    w->type = W_BUTTON;
    w->parent = grid_idx;
    w->visible = 1;
    w->focusable = 1;
    w->bg = COL_BG;
    w->fg = COL_TEXT;
    w->border_c = COL_BORDER;
    w->border = 1;
    w->radius = 10;
    w->pad_t = w->pad_b = 12;
    w->pad_l = w->pad_r = 14;
    w->bg_set = 1;
    w->fg_set = 1;
    w->col_span = 1;
    w->row_span = 1;
    w->xu = w->yu = w->wu = w->hu = UNIT_AUTO;
    str_copy(w->component, sizeof(w->component), "tile");
    grid->children[grid->child_count++] = idx;
    return idx;
}

int pkg_manager_fill_grid(Screen *s, const char *grid_id, const char *filter, const char *shell_id)
{
    if (!s || !grid_id || !grid_id[0])
        return -1;
    if (!g_pkg_ready)
        pkg_manager_init();
    Widget *grid = screen_find(s, grid_id);
    if (!grid || grid->type != W_GRID)
        return -1;
    int grid_idx = (int)(grid - s->widgets);
    for (int i = 0; i < grid->child_count; i++) {
        Widget *c = &s->widgets[grid->children[i]];
        c->visible = 0;
        c->focusable = 0;
        c->text[0] = 0;
        c->action[0] = 0;
        c->icon[0] = 0;
    }
    int slot = 0;
    int placed = 0;
    for (int i = 0; i < g_pkg_reg.count; i++) {
        const PkgApp *app = &g_pkg_reg.apps[i];
        if (shell_id && shell_id[0] && str_eq(app->id, shell_id))
            continue;
        if (str_eq(app->id, "launcher") || str_eq(app->id, "init") || str_eq(app->id, "pid1"))
            continue;
        if (!pkg_str_has(app->name, filter) && !pkg_str_has(app->id, filter))
            continue;
        Widget *btn = NULL;
        while (slot < grid->child_count) {
            Widget *c = &s->widgets[grid->children[slot]];
            slot++;
            if (c->type == W_BUTTON) {
                btn = c;
                break;
            }
        }
        if (!btn) {
            int ni = pkg_alloc_button(s, grid, grid_idx);
            if (ni < 0)
                break;
            btn = &s->widgets[ni];
            slot = grid->child_count;
        }
        str_copy(btn->text, sizeof(btn->text), app->name[0] ? app->name : app->id);
        str_copy(btn->icon, sizeof(btn->icon), app->icon[0] ? app->icon : app->id);
        if (!btn->icon_set[0])
            str_copy(btn->icon_set, sizeof(btn->icon_set), "lucide");
        snprintf(btn->action, sizeof(btn->action), "app:%s", app->id);
        btn->bg = COL_BG;
        btn->fg = COL_TEXT;
        btn->border_c = COL_BORDER;
        btn->border = 1;
        btn->radius = 10;
        btn->bg_set = 1;
        btn->fg_set = 1;
        btn->visible = 1;
        btn->focusable = 1;
        placed++;
    }
    if (grid->grid_cols <= 0)
        grid->grid_cols = 3;
    return placed;
}

#ifndef VILAOS_PKG_H
#define VILAOS_PKG_H

#include "types.h"

#define PKG_MAX_APPS 32
#define PKG_MAX_CAPS 16
#define PKG_MAX_VER 16
#define PKG_ID_LEN 32
#define PKG_NAME_LEN 48
#define PKG_APPS_ROOT "/apps"
#define PKG_DATA_ROOT "/data/apps"
#define PKG_REG_PATH "/var/lib/vilaos/registry.ini"
#define PKG_CAP_TELEPHONY     (1u << 0)
#define PKG_CAP_MESSAGES      (1u << 1)
#define PKG_CAP_CONTACTS      (1u << 2)
#define PKG_CAP_STORAGE       (1u << 3)
#define PKG_CAP_CLOCK         (1u << 4)
#define PKG_CAP_DISPLAY       (1u << 5)
#define PKG_CAP_NETWORK       (1u << 6)
#define PKG_CAP_AUDIO         (1u << 7)
#define PKG_CAP_UI            (1u << 8)
#define PKG_CAP_STORAGE_FILES (1u << 9)
#define PKG_CAP_SYSTEM_POWER  (1u << 10)
#define PKG_CAP_SYSTEM_DEVICE (1u << 11)
#define PKG_CAP_STORAGE_APP   PKG_CAP_STORAGE
#define PKG_CAP_AUDIO_BG      PKG_CAP_AUDIO
#define PKG_CAP_SYSTEM_CLOCK  PKG_CAP_CLOCK

typedef struct {
    char id[PKG_ID_LEN];
    char name[PKG_NAME_LEN];
    char version[PKG_MAX_VER];
    int version_code;
    char icon[MAX_ID];
    char entry_screen[MAX_PATH];
    char entry_script[MAX_PATH];
    char summary[MAX_TEXT];
    unsigned caps;
    int installed;
} PkgApp;

typedef struct {
    PkgApp apps[PKG_MAX_APPS];
    int count;
} PkgRegistry;

unsigned pkg_cap_from_name(const char *name);
int pkg_parse_manifest(const char *path, PkgApp *out);
int pkg_registry_load(PkgRegistry *reg);
int pkg_registry_save(const PkgRegistry *reg);
int pkg_registry_find(const PkgRegistry *reg, const char *id);
int pkg_install(const char *vpkg_path, PkgRegistry *reg);
int pkg_update(const char *vpkg_path, PkgRegistry *reg);
int pkg_remove(const char *id, PkgRegistry *reg);
int pkg_list(const PkgRegistry *reg, char *out, size_t out_sz);
int pkg_app_path(const char *id, char *out, size_t n);
int pkg_data_path(const char *id, char *out, size_t n);
int pkg_seed_from_dir(const char *apps_src_root, PkgRegistry *reg);

typedef void (*PkgChangedFn)(void *user);

int pkg_manager_init(void);
int pkg_manager_refresh(void);
const PkgRegistry *pkg_manager_registry(void);
int pkg_manager_count(void);
int pkg_manager_list_json(char *out, size_t out_n);
int pkg_manager_get(int index, PkgApp *out);
int pkg_manager_find(const char *id, PkgApp *out);
int pkg_manager_on_changed(PkgChangedFn fn, void *user);
int pkg_manager_poll_changed(void);
void pkg_manager_notify_changed(void);
int pkg_manager_fill_grid(Screen *s, const char *grid_id, const char *filter, const char *shell_id);
int pkg_manager_caps_text(unsigned caps, char *buf, size_t n);

#endif

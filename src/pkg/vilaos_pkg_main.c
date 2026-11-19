#include "pkg.h"
#include "util.h"

#include <stdio.h>
#include <string.h>

static void usage(const char *argv0)
{
    fprintf(stderr, "Usage: %s install|update|remove|list [pkg|id]\n", argv0);
}

int main(int argc, char **argv)
{
    if (argc < 2) {
        usage(argv[0]);
        return 1;
    }
    PkgRegistry reg;
    pkg_registry_load(&reg);

    if (str_eq(argv[1], "list")) {
        char buf[2048];
        pkg_list(&reg, buf, sizeof(buf));
        fputs(buf[0] ? buf : "(no apps)\n", stdout);
        return 0;
    }
    if (str_eq(argv[1], "install") || str_eq(argv[1], "update")) {
        if (argc < 3) {
            usage(argv[0]);
            return 1;
        }
        int rc = str_eq(argv[1], "update") ? pkg_update(argv[2], &reg) : pkg_install(argv[2], &reg);
        if (rc != 0) {
            fprintf(stderr, "failed to %s %s\n", argv[1], argv[2]);
            return 1;
        }
        printf("%s ok\n", argv[1]);
        return 0;
    }
    if (str_eq(argv[1], "remove")) {
        if (argc < 3) {
            usage(argv[0]);
            return 1;
        }
        if (pkg_remove(argv[2], &reg) != 0) {
            fprintf(stderr, "failed to remove %s\n", argv[2]);
            return 1;
        }
        printf("removed %s\n", argv[2]);
        return 0;
    }
    usage(argv[0]);
    return 1;
}

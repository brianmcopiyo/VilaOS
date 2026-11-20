#include "util.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#include <direct.h>
#include <windows.h>
#else
#include <unistd.h>
#include <pwd.h>
#endif

char *trim(char *s)
{
    char *end;

    if (s == NULL) {
        return NULL;
    }

    while (*s && isspace((unsigned char)*s)) {
        s++;
    }

    if (*s == '\0') {
        return s;
    }

    end = s + strlen(s) - 1;
    while (end > s && isspace((unsigned char)*end)) {
        end--;
    }
    end[1] = '\0';

    return s;
}

int get_cwd(char *buf, size_t size)
{
    if (buf == NULL || size == 0) {
        return -1;
    }

#ifdef _WIN32
    if (_getcwd(buf, (int)size) == NULL) {
        return -1;
    }
#else
    if (getcwd(buf, size) == NULL) {
        return -1;
    }
#endif

    return 0;
}

void format_prompt(char *buf, size_t size)
{
    char user[256] = "user";
    char host[256] = "host";
    char cwd[PATH_MAX_LEN] = ".";

    if (buf == NULL || size == 0) {
        return;
    }

#ifdef _WIN32
    {
        DWORD n = (DWORD)sizeof(user);
        if (!GetUserNameA(user, &n)) {
            strncpy(user, "user", sizeof(user) - 1);
        }
        n = (DWORD)sizeof(host);
        if (!GetComputerNameA(host, &n)) {
            strncpy(host, "host", sizeof(host) - 1);
        }
    }
#else
    {
        struct passwd *pw = getpwuid(getuid());
        if (pw && pw->pw_name) {
            strncpy(user, pw->pw_name, sizeof(user) - 1);
            user[sizeof(user) - 1] = '\0';
        }
        if (gethostname(host, sizeof(host)) != 0) {
            strncpy(host, "host", sizeof(host) - 1);
        }
        host[sizeof(host) - 1] = '\0';
    }
#endif

    if (get_cwd(cwd, sizeof(cwd)) != 0) {
        strncpy(cwd, ".", sizeof(cwd) - 1);
    }

    snprintf(buf, size, "%s@%s:%s$ ", user, host, cwd);
}

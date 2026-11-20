#include "builtins.h"
#include "util.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <direct.h>
#else
#include <unistd.h>
#endif

static void print_help(void)
{
    puts("Built-in commands:");
    puts("  help   List built-in commands");
    puts("  cd     Change directory");
    puts("  pwd    Print working directory");
    puts("  clear  Clear the screen");
    puts("  exit   Quit the shell");
}

static int builtin_cd(int argc, char **argv)
{
    const char *path;

    if (argc < 2) {
        path = getenv("USERPROFILE");
        if (path == NULL) {
            path = getenv("HOME");
        }
        if (path == NULL) {
            fputs("cd: no home directory\n", stderr);
            return 1;
        }
    } else {
        path = argv[1];
    }

#ifdef _WIN32
    if (_chdir(path) != 0) {
#else
    if (chdir(path) != 0) {
#endif
        perror("cd");
        return 1;
    }

    return 0;
}

static int builtin_pwd(void)
{
    char cwd[PATH_MAX_LEN];

    if (get_cwd(cwd, sizeof(cwd)) != 0) {
        perror("pwd");
        return 1;
    }

    puts(cwd);
    return 0;
}

static int builtin_clear(void)
{
#ifdef _WIN32
    system("cls");
#else
    system("clear");
#endif
    return 0;
}

int try_builtin(int argc, char **argv, int *should_exit)
{
    const char *cmd;

    if (should_exit != NULL) {
        *should_exit = 0;
    }

    if (argc < 1 || argv == NULL || argv[0] == NULL) {
        return 0;
    }

    cmd = argv[0];

    if (strcmp(cmd, "help") == 0) {
        print_help();
        return 1;
    }

    if (strcmp(cmd, "cd") == 0) {
        builtin_cd(argc, argv);
        return 1;
    }

    if (strcmp(cmd, "pwd") == 0) {
        builtin_pwd();
        return 1;
    }

    if (strcmp(cmd, "clear") == 0) {
        builtin_clear();
        return 1;
    }

    if (strcmp(cmd, "exit") == 0) {
        if (should_exit != NULL) {
            *should_exit = 1;
        }
        return 1;
    }

    return 0;
}

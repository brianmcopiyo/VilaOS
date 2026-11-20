#include "exec.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#endif

#ifdef _WIN32

/* Escape and join argv into a single CreateProcess command line. */
static char *build_cmdline(int argc, char **argv)
{
    size_t total = 0;
    char *cmd;
    char *out;
    int i;

    for (i = 0; i < argc; i++) {
        size_t len = strlen(argv[i]);
        /* Quotes + possible escapes + separator space */
        total += len * 2 + 4;
    }
    total += 1;

    cmd = (char *)malloc(total);
    if (cmd == NULL) {
        return NULL;
    }

    out = cmd;
    for (i = 0; i < argc; i++) {
        const char *arg = argv[i];
        int needs_quotes = 0;
        const char *p;

        if (i > 0) {
            *out++ = ' ';
        }

        for (p = arg; *p != '\0'; p++) {
            if (*p == ' ' || *p == '\t' || *p == '"') {
                needs_quotes = 1;
                break;
            }
        }

        if (!needs_quotes) {
            size_t len = strlen(arg);
            memcpy(out, arg, len);
            out += len;
            continue;
        }

        *out++ = '"';
        for (p = arg; *p != '\0'; p++) {
            if (*p == '"') {
                *out++ = '\\';
            }
            *out++ = *p;
        }
        *out++ = '"';
    }
    *out = '\0';

    return cmd;
}

int run_external(int argc, char **argv)
{
    char *cmdline;
    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    DWORD exit_code = 1;
    BOOL ok;

    if (argc < 1 || argv == NULL || argv[0] == NULL) {
        return -1;
    }

    cmdline = build_cmdline(argc, argv);
    if (cmdline == NULL) {
        fputs("error: out of memory\n", stderr);
        return -1;
    }

    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    ZeroMemory(&pi, sizeof(pi));

    ok = CreateProcessA(
        NULL,
        cmdline,
        NULL,
        NULL,
        TRUE,
        0,
        NULL,
        NULL,
        &si,
        &pi
    );

    free(cmdline);

    if (!ok) {
        fprintf(stderr, "%s: command not found or failed to start (%lu)\n",
                argv[0], (unsigned long)GetLastError());
        return -1;
    }

    WaitForSingleObject(pi.hProcess, INFINITE);
    GetExitCodeProcess(pi.hProcess, &exit_code);

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    return (int)exit_code;
}

#else

#include <sys/wait.h>
#include <unistd.h>

int run_external(int argc, char **argv)
{
    pid_t pid;
    int status;

    (void)argc;

    if (argv == NULL || argv[0] == NULL) {
        return -1;
    }

    pid = fork();
    if (pid < 0) {
        perror("fork");
        return -1;
    }

    if (pid == 0) {
        execvp(argv[0], argv);
        perror(argv[0]);
        _exit(127);
    }

    if (waitpid(pid, &status, 0) < 0) {
        perror("waitpid");
        return -1;
    }

    if (WIFEXITED(status)) {
        return WEXITSTATUS(status);
    }

    return -1;
}

#endif

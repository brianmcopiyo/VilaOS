#include "parser.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

static char *dup_range(const char *start, size_t len)
{
    char *out = (char *)malloc(len + 1);
    if (out == NULL) {
        return NULL;
    }
    memcpy(out, start, len);
    out[len] = '\0';
    return out;
}

char **parse_line(const char *line, int *argc_out)
{
    char **argv;
    int argc = 0;
    const char *p;

    if (argc_out == NULL) {
        return NULL;
    }
    *argc_out = 0;

    if (line == NULL) {
        return NULL;
    }

    argv = (char **)calloc(MAX_ARGS + 1, sizeof(char *));
    if (argv == NULL) {
        return NULL;
    }

    p = line;
    while (*p != '\0' && argc < MAX_ARGS) {
        const char *start;
        size_t len;
        char *token;
        int in_quote = 0;

        while (*p != '\0' && isspace((unsigned char)*p)) {
            p++;
        }
        if (*p == '\0') {
            break;
        }

        if (*p == '"') {
            in_quote = 1;
            p++;
            start = p;
            while (*p != '\0' && *p != '"') {
                p++;
            }
            len = (size_t)(p - start);
            if (*p == '"') {
                p++;
            }
        } else {
            start = p;
            while (*p != '\0' && !isspace((unsigned char)*p) && *p != '"') {
                p++;
            }
            len = (size_t)(p - start);
            (void)in_quote;
        }

        token = dup_range(start, len);
        if (token == NULL) {
            free_argv(argv);
            return NULL;
        }
        argv[argc++] = token;
    }

    argv[argc] = NULL;
    *argc_out = argc;

    if (argc == 0) {
        free(argv);
        return NULL;
    }

    return argv;
}

void free_argv(char **argv)
{
    int i;

    if (argv == NULL) {
        return;
    }

    for (i = 0; argv[i] != NULL; i++) {
        free(argv[i]);
    }
    free(argv);
}

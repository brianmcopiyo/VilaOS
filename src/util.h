#ifndef UTIL_H
#define UTIL_H

#include <stddef.h>

#define LINE_MAX 4096
#define PATH_MAX_LEN 1024
#define PROMPT_MAX 1280

/* Trim leading/trailing whitespace in place. Returns s. */
char *trim(char *s);

/* Write prompt into buf (size bytes). Format: user@host:cwd$  */
void format_prompt(char *buf, size_t size);

/* Copy current working directory into buf. Returns 0 on success. */
int get_cwd(char *buf, size_t size);

#endif /* UTIL_H */

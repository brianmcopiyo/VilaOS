#ifndef PARSER_H
#define PARSER_H

#define MAX_ARGS 64

/*
 * Tokenize line into argv-style array.
 * Supports whitespace separation and "double-quoted" arguments.
 * On success: sets *argc_out, returns malloc'd NULL-terminated argv
 *             (each string and the array itself must be freed with free_argv).
 * On failure or empty input: returns NULL and *argc_out = 0.
 */
char **parse_line(const char *line, int *argc_out);

/* Free argv returned by parse_line. */
void free_argv(char **argv);

#endif /* PARSER_H */

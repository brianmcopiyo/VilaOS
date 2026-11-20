#ifndef EXEC_H
#define EXEC_H

/*
 * Run an external command via CreateProcess.
 * argv is NULL-terminated; argc is the argument count.
 * Returns the process exit code, or -1 on launch failure.
 */
int run_external(int argc, char **argv);

#endif /* EXEC_H */

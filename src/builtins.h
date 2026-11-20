#ifndef BUILTINS_H
#define BUILTINS_H

/*
 * If argv[0] is a built-in, run it and return 1.
 * Sets *should_exit to 1 if the shell should quit.
 * Returns 0 if not a built-in.
 */
int try_builtin(int argc, char **argv, int *should_exit);

#endif /* BUILTINS_H */

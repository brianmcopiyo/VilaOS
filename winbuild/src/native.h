#ifndef NATIVE_H
#define NATIVE_H

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Boots the embedded WebView UI (React from bin/ui) and runs until quit.
 * Low-level C services (pwd/cd/run) are exposed to the frontend.
 * Returns process exit code.
 */
int native_run(void);

#ifdef __cplusplus
}
#endif

#endif /* NATIVE_H */

// crt_externs.h for Argon on Linux: the process arguments, captured when libargonobjc loads
#pragma once
#ifdef __cplusplus
extern "C" {
#endif
int *_NSGetArgc(void);
char ***_NSGetArgv(void);
char ***_NSGetEnviron(void);
char **_NSGetProgname(void);
#ifdef __cplusplus
}
#endif

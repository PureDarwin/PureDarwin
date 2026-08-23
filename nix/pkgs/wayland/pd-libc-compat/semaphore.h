/* <semaphore.h> with working unnamed semaphores.
 *
 * Darwin ships sem_init/sem_destroy as deprecated stubs that always fail with
 * ENOSYS, and PureDarwin's libSystem exports none of the named-semaphore
 * family either. The replacements are backed by Grand Central Dispatch's
 * counting semaphore, which is exported and needs no name.
 *
 * The implementation deliberately lives in libepoll-shim rather than here:
 * sem_t is an int, so it can only hold an index into a table of real dispatch
 * semaphores, and that table has to be shared by every translation unit. As
 * static inlines each file got its own copy, so a sem_wait in one file could
 * not see a sem_init from another - it returned -1 without blocking, and
 * callers that ignore the return value ran straight through.
 *
 * Anything using this header must therefore link -lepoll-shim.
 */
#ifndef PD_SEMAPHORE_COMPAT_H
#define PD_SEMAPHORE_COMPAT_H

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wgnu-include-next"
#include_next <semaphore.h>
#pragma clang diagnostic pop

#ifdef __cplusplus
extern "C" {
#endif

int pd_sem_init(sem_t *sem, int pshared, unsigned int value);
int pd_sem_wait(sem_t *sem);
int pd_sem_trywait(sem_t *sem);
int pd_sem_post(sem_t *sem);
int pd_sem_destroy(sem_t *sem);

#ifdef __cplusplus
}
#endif

#define sem_init(s, p, v)  pd_sem_init((s), (p), (v))
#define sem_destroy(s)     pd_sem_destroy(s)
#define sem_wait(s)        pd_sem_wait(s)
#define sem_trywait(s)     pd_sem_trywait(s)
#define sem_post(s)        pd_sem_post(s)

#endif /* PD_SEMAPHORE_COMPAT_H */

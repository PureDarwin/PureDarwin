#ifdef OPENLIBM_USE_HOST_FENV_H
#include <fenv.h>
#else /* !OPENLIBM_USE_HOST_FENV_H */

#if defined(__aarch64__) || defined(__arm__)
#include <openlibm_fenv_arm.h>
#elif defined(__x86_64__)
#include <openlibm_fenv_amd64.h>
#elif defined(__i386__)
#include <openlibm_fenv_i387.h>
#elif defined(__powerpc__)
#include <openlibm_fenv_powerpc.h>
#else
#error "Unsupported platform"
#endif

#endif /* OPENLIBM_USE_HOST_FENV_H */

/*
 * Force-included ahead of every libdarwin source. Libc's own build reaches
 * these files with the C library and mach basics already in scope;
 * os/assumes.h and libdarwin's h/stdio.h use FILE, strerror() and the mach
 * types without including anything that declares them.
 */

#ifndef _PUREDARWIN_LIBDARWIN_PRELUDE_H_
#define _PUREDARWIN_LIBDARWIN_PRELUDE_H_

#include <stdio.h>
#include <string.h>
#include <mach/mach.h>
/* pthread/spawn.h uses posix_spawnattr_t without including <spawn.h>. */
#include <spawn.h>

#endif /* _PUREDARWIN_LIBDARWIN_PRELUDE_H_ */

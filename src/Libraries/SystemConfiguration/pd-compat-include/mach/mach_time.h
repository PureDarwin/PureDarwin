/*
 * libc/include's <mach/mach_time.h> has to win the header search here (its
 * neighbours <string.h> and <mach/mach.h> are the userspace copies, while
 * osfmk's are kernel-side), but it is the one copy that does not declare
 * mach_get_times() - only osfmk's does. Chain to it and add that declaration.
 */

#ifndef _PUREDARWIN_SC_MACH_TIME_H_
#define _PUREDARWIN_SC_MACH_TIME_H_

#include_next <mach/mach_time.h>

#include <time.h>

__BEGIN_DECLS

kern_return_t mach_get_times(uint64_t *absolute_time,
    uint64_t *cont_time,
    struct timespec *tp);

__END_DECLS

#endif /* _PUREDARWIN_SC_MACH_TIME_H_ */

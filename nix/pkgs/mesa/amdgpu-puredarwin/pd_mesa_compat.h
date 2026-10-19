// SPDX-License-Identifier: MIT
// Linux interfaces Mesa's drm paths use that Darwin lacks, force-included
// into the amdgpu build
#ifndef PD_MESA_COMPAT_H
#define PD_MESA_COMPAT_H

#include <poll.h>
#include <signal.h>
#include <time.h>

// poll with a timespec timeout rounded up to whole milliseconds
// the signal mask stays as it is, no caller in mesa relies on swapping it
static inline int
ppoll(struct pollfd *fds, nfds_t nfds, const struct timespec *timeout,
      const sigset_t *sigmask)
{
   int ms = -1;

   (void)sigmask;
   if (timeout != NULL) {
      long long t = (long long)timeout->tv_sec * 1000 + (timeout->tv_nsec + 999999) / 1000000;
      ms = t > 0x7fffffff ? 0x7fffffff : (int)t;
   }
   return poll(fds, nfds, ms);
}

#endif

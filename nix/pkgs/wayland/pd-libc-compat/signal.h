/* <signal.h> plus the realtime-signal bounds.
 *
 * Darwin has no POSIX realtime signals, so SIGRTMIN/SIGRTMAX do not exist.
 * Ports use them as loop bounds when resetting every handler; NSIG is the
 * equivalent bound here, and an empty range for SIGRTMIN..SIGRTMAX is the
 * honest description of a system with no realtime signals.
 */
#ifndef PD_SIGNAL_COMPAT_H
#define PD_SIGNAL_COMPAT_H

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wgnu-include-next"
#include_next <signal.h>
#pragma clang diagnostic pop

#ifndef SIGRTMIN
#define SIGRTMIN NSIG
#endif
#ifndef SIGRTMAX
#define SIGRTMAX NSIG
#endif

#endif /* PD_SIGNAL_COMPAT_H */

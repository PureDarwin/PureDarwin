/*
 * PureDarwin: setregid(2) is marked NO_SYSCALL_STUB in syscalls.master
 * (real Apple hides it from the public API, though the kernel trap - and
 * the kauth_cred_setresgid() logic behind it - is present). See
 * __setreuid.s for the matching rationale.
 */

#include "SYS.h"

#if defined(__x86_64__)

__SYSCALL(___setregid, setregid, 2)

#elif defined(__i386__)

__SYSCALL_INT(___setregid, setregid, 2)

#elif defined(__arm__)

__SYSCALL(___setregid, setregid, 2)

#elif defined(__arm64__)

__SYSCALL(___setregid, setregid, 2)

#else
#error Unsupported architecture
#endif

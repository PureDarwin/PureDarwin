/*
 * PureDarwin: setreuid(2) is marked NO_SYSCALL_STUB in syscalls.master
 * (real Apple hides it from the public API, though the kernel trap - and
 * the kauth_cred_setresuid() logic behind it - is present), so no automatic
 * stub exists to base a real getresuid/setresuid emulation on. This is the
 * plain two-argument raw-syscall trampoline, in the same style as the
 * adjacent auto-generated stubs (e.g. __sigaltstack.s).
 */

#include "SYS.h"

#if defined(__x86_64__)

__SYSCALL(___setreuid, setreuid, 2)

#elif defined(__i386__)

__SYSCALL_INT(___setreuid, setreuid, 2)

#elif defined(__arm__)

__SYSCALL(___setreuid, setreuid, 2)

#elif defined(__arm64__)

__SYSCALL(___setreuid, setreuid, 2)

#else
#error Unsupported architecture
#endif

/*
 * PureDarwin: open_dprotected_np(2) is marked NO_SYSCALL_STUB in
 * syscalls.master, so no stub is generated for it even though the kernel trap
 * exists. configd's SCPCommit.c calls it to create preference files with a
 * data-protection class.
 *
 * mach/open_dprotected_np.c is the real wrapper and already calls
 * ___open_dprotected_np; only the trap stub behind it was missing. Five-argument
 * raw-syscall trampoline, in the same style as the adjacent __setreuid.s.
 */

#include "SYS.h"

#if defined(__x86_64__)

__SYSCALL(___open_dprotected_np, open_dprotected_np, 5)

#elif defined(__i386__)

__SYSCALL_INT(___open_dprotected_np, open_dprotected_np, 5)

#elif defined(__arm__)

__SYSCALL(___open_dprotected_np, open_dprotected_np, 5)

#elif defined(__arm64__)

__SYSCALL(___open_dprotected_np, open_dprotected_np, 5)

#else
#error Unsupported architecture
#endif

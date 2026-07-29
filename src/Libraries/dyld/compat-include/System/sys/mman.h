#ifndef PUREDARWIN_DYLD_COMPAT_SYSTEM_SYS_MMAN_H
#define PUREDARWIN_DYLD_COMPAT_SYSTEM_SYS_MMAN_H

#include <sys/mman.h>

/* XNU exposes this syscall to dyld, while the SDK's public header omits it. */
#ifndef PUREDARWIN_MREMAP_ENCRYPTED_DECL
#define PUREDARWIN_MREMAP_ENCRYPTED_DECL
#include <stdint.h>
int mremap_encrypted(void *, size_t, uint32_t, uint32_t, uint32_t);
#endif

#endif /* PUREDARWIN_DYLD_COMPAT_SYSTEM_SYS_MMAN_H */

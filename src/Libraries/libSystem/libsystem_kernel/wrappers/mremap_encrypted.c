/*
 * The syscall is part of the Darwin userspace ABI even when code
 * decryption is unavailable. dyld calls it for encrypted Mach-O images;
 * unencrypted images must be accepted without requiring a crypto service.
 */
#include <errno.h>
#include <stddef.h>
#include <stdint.h>

int
mremap_encrypted(void *addr, size_t len, uint32_t cryptid,
    uint32_t cputype, uint32_t cpusubtype)
{
	(void)addr;
	(void)len;
	(void)cputype;
	(void)cpusubtype;

	if (cryptid == 0) {
		return 0;
	}

	errno = ENOTSUP;
	return -1;
}

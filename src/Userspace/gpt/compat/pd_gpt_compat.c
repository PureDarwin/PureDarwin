// the two host pieces gpt needs that Darwin spells differently: opening a disk and its size in bytes
#include <errno.h>
#include <inttypes.h>
#include <stdlib.h>
#include <vis.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/disk.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include "nbtool_config.h"
#include "opendisk.h"

#undef ioctl

int
pd_gpt_ioctl(int fd, unsigned long request, void *arg)
{
	if (request == DIOCGMEDIASIZE) {
		uint32_t bs = 0;
		uint64_t count = 0;

		if (ioctl(fd, DKIOCGETBLOCKSIZE, &bs) == -1 || ioctl(fd, DKIOCGETBLOCKCOUNT, &count) == -1)
			return -1;
		*(off_t *)arg = (off_t)(count * bs);
		return 0;
	}
	return ioctl(fd, request, arg);
}

// a path opens as given, a bare name like disk1 is tried as the raw device first
int
opendisk(const char *path, int flags, char *buf, size_t buflen, int iscooked)
{
	int fd;

	snprintf(buf, buflen, "%s", path);
	fd = open(buf, flags);
	if (fd != -1 || strchr(path, '/') != NULL)
		return fd;
	if (!iscooked) {
		snprintf(buf, buflen, "/dev/r%s", path);
		fd = open(buf, flags);
		if (fd != -1)
			return fd;
	}
	snprintf(buf, buflen, "/dev/%s", path);
	return open(buf, flags);
}

// NetBSD's strtou: strtoumax that clamps to [lo, hi] and reports what went wrong through rstatus
uintmax_t
strtou(const char *nptr, char **endptr, int base, uintmax_t lo, uintmax_t hi, int *rstatus)
{
	char *end;
	int saved = errno, status = 0;
	uintmax_t v;

	errno = 0;
	v = strtoumax(nptr, &end, base);
	if (end == nptr)
		status = ECANCELED;
	else if (errno == ERANGE)
		status = ERANGE;
	else if (*end != '\0')
		status = ENOTSUP;
	if (v < lo) {
		v = lo;
		status = status ? status : ERANGE;
	} else if (v > hi) {
		v = hi;
		status = status ? status : ERANGE;
	}
	if (endptr)
		*endptr = end;
	if (rstatus)
		*rstatus = status;
	errno = saved;
	return v;
}

// NetBSD's stravis: strvis into a buffer it allocates, four bytes per input byte is the worst case
int
stravis(char **outp, const char *src, int flags)
{
	char *buf = malloc(strlen(src) * 4 + 1);

	*outp = buf;
	if (buf == NULL)
		return -1;
	return strvis(buf, src, flags);
}

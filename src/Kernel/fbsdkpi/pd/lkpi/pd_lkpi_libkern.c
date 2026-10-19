// SPDX-License-Identifier: MIT
// libkern, time and small kernel helpers linuxkpi and net80211 call
// the haiku compat layer has no copy of these

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/time.h>
#include <sys/libkern.h>

#include <string.h>

#include "../../pd_fbsd_os.h"

static int
digit_value(int c)
{
	if (c >= '0' && c <= '9')
		return c - '0';
	if (c >= 'a' && c <= 'z')
		return c - 'a' + 10;
	if (c >= 'A' && c <= 'Z')
		return c - 'A' + 10;
	return 99;
}

// the shared parser, negative values come back two's complement
static unsigned long long
parse_number(const char *nptr, char **endptr, int base, int *negative)
{
	const char *s = nptr;
	unsigned long long value = 0;
	bool any = false;

	while (*s == ' ' || (*s >= '\t' && *s <= '\r'))
		s++;
	*negative = 0;
	if (*s == '-' || *s == '+') {
		*negative = *s == '-';
		s++;
	}
	if ((base == 0 || base == 16) && s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) {
		s += 2;
		base = 16;
	} else if (base == 0) {
		base = s[0] == '0' ? 8 : 10;
	}

	for (;; s++) {
		int d = digit_value(*s);
		if (d >= base)
			break;
		value = value * base + d;
		any = true;
	}

	if (endptr != NULL)
		*endptr = (char *)(any ? s : nptr);
	return *negative ? -value : value;
}

unsigned long long
strtoull(const char *nptr, char **endptr, int base)
{
	int negative;
	return parse_number(nptr, endptr, base, &negative);
}

long long
strtoll(const char *nptr, char **endptr, int base)
{
	int negative;
	return (long long)parse_number(nptr, endptr, base, &negative);
}

unsigned long
strtoul(const char *nptr, char **endptr, int base)
{
	return (unsigned long)strtoull(nptr, endptr, base);
}

long
strtol(const char *nptr, char **endptr, int base)
{
	return (long)strtoll(nptr, endptr, base);
}

char *
strchrnul(const char *s, int c)
{
	while (*s != '\0' && *s != (char)c)
		s++;
	return (char *)s;
}

unsigned int
arc4random_uniform(unsigned int upper)
{
	if (upper < 2)
		return 0;

	// reject the low values that would bias the modulo
	unsigned int min = -upper % upper;
	unsigned int r;
	do {
		r = arc4random();
	} while (r < min);
	return r % upper;
}

static void
swap_bytes(char *a, char *b, size_t size)
{
	while (size-- > 0) {
		char t = *a;
		*a++ = *b;
		*b++ = t;
	}
}

// a shell sort, the arrays linuxkpi sorts are small
void
qsort_r(void *base, size_t nmemb, size_t size,
	int (*compar)(const void *, const void *, void *), void *thunk)
{
	char *array = base;
	size_t gap, i, j;

	for (gap = nmemb / 2; gap > 0; gap /= 2) {
		for (i = gap; i < nmemb; i++) {
			for (j = i; j >= gap
				&& compar(array + (j - gap) * size, array + j * size, thunk) > 0;
				j -= gap)
				swap_bytes(array + (j - gap) * size, array + j * size, size);
		}
	}
}

static int
qsort_plain(const void *a, const void *b, void *thunk)
{
	int (*compar)(const void *, const void *) = thunk;
	return compar(a, b);
}

void
qsort(void *base, size_t nmemb, size_t size, int (*compar)(const void *, const void *))
{
	qsort_r(base, nmemb, size, qsort_plain, (void *)compar);
}

void *
bsearch(const void *key, const void *base, size_t nmemb, size_t size,
    int (*compar)(const void *, const void *))
{
	size_t lo = 0, hi = nmemb;

	while (lo < hi) {
		size_t mid = lo + (hi - lo) / 2;
		const char *p = (const char *)base + mid * size;
		int c = compar(key, p);

		if (c == 0)
			return (void *)p;
		if (c < 0)
			hi = mid;
		else
			lo = mid + 1;
	}
	return NULL;
}

// the tsc rate, measured once against the uptime clock
unsigned long long
pd_lkpi_tsc_freq(void)
{
	static unsigned long long freq;
	int64_t t0, t1;
	uint64_t c0, c1;

	if (freq != 0)
		return freq;
#if defined(__x86_64__)
	t0 = pdos_uptime_us();
	c0 = __builtin_ia32_rdtsc();
	do {
		t1 = pdos_uptime_us();
	} while (t1 - t0 < 10000);
	c1 = __builtin_ia32_rdtsc();
	freq = (c1 - c0) * 1000000ULL / (uint64_t)(t1 - t0);
#endif
	return freq;
}

void
nanouptime(struct timespec *ts)
{
	int64_t us = pdos_uptime_us();
	ts->tv_sec = us / 1000000;
	ts->tv_nsec = (us % 1000000) * 1000;
}

void
getnanouptime(struct timespec *ts)
{
	nanouptime(ts);
}

// wall time is not kept here, uptime stands in for it
void
nanotime(struct timespec *ts)
{
	nanouptime(ts);
}

void
microtime(struct timeval *tv)
{
	int64_t us = pdos_uptime_us();
	tv->tv_sec = us / 1000000;
	tv->tv_usec = us % 1000000;
}

void *
pd_lkpi_phys_to_dmap(unsigned long long pa)
{
	// there is no direct map, callers check PHYS_IN_DMAP first
	return NULL;
}

int linux_copyin(const void *uaddr, void *kaddr, size_t len);
int linux_copyout(const void *kaddr, void *uaddr, size_t len);

// a fault here never re-enters the driver, gem mmaps are populated when mapped
int
copyin_nofault(const void *uaddr, void *kaddr, size_t len)
{
	return -linux_copyin(uaddr, kaddr, len);
}

int
copyout_nofault(const void *kaddr, void *uaddr, size_t len)
{
	return -linux_copyout(kaddr, uaddr, len);
}

size_t
malloc_size(size_t size)
{
	return size;
}

void
bus_topo_lock(void)
{
}

void
bus_topo_unlock(void)
{
}

void
rt_ieee80211msg(struct ifnet *ifp, int what, void *data, size_t len)
{
}

void
rt_ifmsg(struct ifnet *ifp, int flags)
{
}

void
tdsignal(struct thread *td, int sig)
{
}

void
max_linkhdr_grow(int len)
{
}

unsigned short
in_cksum_hdr(const void *ip)
{
	const unsigned short *w = ip;
	unsigned int hlen = (((const unsigned char *)ip)[0] & 0xf) * 2;
	unsigned int sum = 0;

	while (hlen-- > 0)
		sum += *w++;
	sum = (sum >> 16) + (sum & 0xffff);
	sum += sum >> 16;
	return (unsigned short)~sum;
}

void *
kmem_alloc_contig(unsigned long size, int flags, unsigned long low,
	unsigned long high, unsigned long alignment, unsigned long boundary,
	int memattr)
{
	uint64_t phys;
	void *va = pdos_malloc_contig(size, alignment > PAGE_SIZE ? alignment : PAGE_SIZE,
		high, &phys);
	if (va != NULL && (flags & M_ZERO) != 0)
		memset(va, 0, size);
	return va;
}

void
kmem_free(void *addr, unsigned long size)
{
	pdos_free_contig(addr, size);
}

void *
contigmalloc_domainset(unsigned long size, struct malloc_type *type, void *ds,
	int flags, unsigned long low, unsigned long high, unsigned long alignment,
	unsigned long boundary)
{
	// an area like contigmalloc, so free and kfree can release it
	return _kernel_contigmalloc(__FILE__, __LINE__, size, flags, low, high, alignment,
		boundary);
}

unsigned long
pd_lkpi_jiffies(void)
{
	return (unsigned long)(pdos_uptime_us() / 1000);
}

// interrupt handlers run on their own threads, nothing to wait out here
void
_intr_drain(int irq)
{
}

int
abs(int v)
{
	return v < 0 ? -v : v;
}

int
vasprintf(char **ret, struct malloc_type *mtp, const char *format, __builtin_va_list ap)
{
	__builtin_va_list copy;
	__builtin_va_copy(copy, ap);
	int len = vsnprintf(NULL, 0, format, copy);
	__builtin_va_end(copy);

	char *buffer = malloc(len + 1, M_DEVBUF, M_NOWAIT);
	if (buffer == NULL) {
		*ret = NULL;
		return -1;
	}
	vsnprintf(buffer, len + 1, format, ap);
	*ret = buffer;
	return len;
}

// hexdump(9), sixteen bytes a line with the offset, flags are ignored
void
hexdump(const void *ptr, int length, const char *hdr, int flags)
{
	const unsigned char *p = ptr;

	for (int i = 0; i < length; i += 16) {
		char line[16 * 3 + 1];
		int n = 0;
		for (int j = i; j < length && j < i + 16; j++)
			n += snprintf(line + n, sizeof(line) - n, " %02x", p[j]);
		line[n] = '\0';
		printf("%s%04x%s\n", hdr != NULL ? hdr : "", i, line);
	}
}

#if defined(__aarch64__)
// xnu exports these for x86_64 kexts only
char *
strcpy(char *dst, const char *src)
{
	char *d = dst;

	while ((*d++ = *src++) != 0)
		;
	return dst;
}

int
sprintf(char *buf, const char *format, ...)
{
	__builtin_va_list ap;
	int len;

	__builtin_va_start(ap, format);
	len = vsnprintf(buf, INT_MAX, format, ap);
	__builtin_va_end(ap);
	return len;
}
#endif

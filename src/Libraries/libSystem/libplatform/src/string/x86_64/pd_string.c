// x86_64 memmove/memcpy, memset, bzero and memset_pattern{4,8,16} for PureDarwin's libplatform. Large blocks use rep movsb/stosb when the CPU has ERMS (pre-ERMS parts run it as slow microcode),
// everything else is 16-byte vector (SSE2) moves with overlapping head and tail stores.

#include <platform/string.h>

#include <stdint.h>

#if _PLATFORM_OPTIMIZED_MEMMOVE

// 16-byte SSE2 vectors without <emmintrin.h> (libplatform builds -nostdinc), unaligned like movdqu
typedef unsigned char v16 __attribute__((vector_size(16), aligned(1)));

// unaligned scalar accesses
typedef uint64_t u64u __attribute__((aligned(1)));
typedef uint32_t u32u __attribute__((aligned(1)));

// from here on rep movsb/stosb beats the vector loops (start-up cost amortised)
#define PD_REP_THRESHOLD 1024

// clang must not turn these loops back into calls to the functions they implement
#define PD_NO_BUILTIN __attribute__((no_builtin("memcpy", "memmove", "memset")))

// CPUID.(EAX=7,ECX=0):EBX bit 9, read once, the commpage kHasENFSTRG bit is Intel-only
static int pd_erms = -1;

static inline int
has_erms(void)
{
	int erms = __atomic_load_n(&pd_erms, __ATOMIC_RELAXED);

	if (erms < 0) {
		unsigned int a, b, c, d;

		__asm__("cpuid" : "=a"(a), "=b"(b), "=c"(c), "=d"(d) : "a"(0));
		erms = 0;
		if (a >= 7) {
			__asm__("cpuid" : "=a"(a), "=b"(b), "=c"(c), "=d"(d) : "a"(7), "c"(0));
			erms = (b >> 9) & 1;
		}
		__atomic_store_n(&pd_erms, erms, __ATOMIC_RELAXED);
	}
	return erms;
}

static inline void
rep_movsb(void *dst, const void *src, size_t n)
{
	__asm__ volatile("rep movsb" : "+D"(dst), "+S"(src), "+c"(n) : : "memory");
}

static inline void
rep_stosb(void *dst, int c, size_t n)
{
	__asm__ volatile("rep stosb" : "+D"(dst), "+c"(n) : "a"(c) : "memory");
}

// n < 16, any overlap: the loads all happen before the stores
static inline PD_NO_BUILTIN void
move_small(unsigned char *d, const unsigned char *s, size_t n)
{
	if (n >= 8) {
		uint64_t a = *(const u64u *)s;
		uint64_t b = *(const u64u *)(s + n - 8);

		*(u64u *)d = a;
		*(u64u *)(d + n - 8) = b;
	} else if (n >= 4) {
		uint32_t a = *(const u32u *)s;
		uint32_t b = *(const u32u *)(s + n - 4);

		*(u32u *)d = a;
		*(u32u *)(d + n - 4) = b;
	} else if (n) {
		unsigned char a = s[0], b = s[n >> 1], c = s[n - 1];

		d[0] = a, d[n >> 1] = b, d[n - 1] = c;
	}
}

PD_NO_BUILTIN void *
_platform_memmove(void *dst, const void *src, size_t n)
{
	unsigned char *d = dst;
	const unsigned char *s = src;

	if (n < 16) {
		move_small(d, s, n);
		return dst;
	}
	if (n <= 32) {
		v16 a = *(const v16 *)s, b = *(const v16 *)(s + n - 16);

		*(v16 *)d = a;
		*(v16 *)(d + n - 16) = b;
		return dst;
	}
	if ((uintptr_t)d - (uintptr_t)s >= n) {
		// forward is safe: no overlap, or the destination starts below the source
		if (n >= PD_REP_THRESHOLD && has_erms()) {
			rep_movsb(d, s, n);
			return dst;
		}
		v16 tail = *(const v16 *)(s + n - 16);
		unsigned char *end = d + n - 16;

		while (d < end) {
			*(v16 *)d = *(const v16 *)s;
			d += 16, s += 16;
		}
		*(v16 *)end = tail;
		return dst;
	}

	// the destination overlaps the source from above: copy downwards, the head saved first
	v16 head = *(const v16 *)s;
	const unsigned char *sp = s + n;
	unsigned char *dp = d + n;

	while (dp - d > 16) {
		sp -= 16, dp -= 16;
		*(v16 *)dp = *(const v16 *)sp;
	}
	*(v16 *)d = head;
	return dst;
}

// fills n bytes with a 16-byte pattern v, the pattern restarting at d
static inline PD_NO_BUILTIN void
fill16(unsigned char *d, v16 v, size_t n)
{
	unsigned char *end = d + (n & ~(size_t)15);
	unsigned char tail[16];

	while (d < end) {
		*(v16 *)d = v;
		d += 16;
	}
	*(v16 *)tail = v;
	for (size_t i = 0; i < (n & 15); i++) {
		d[i] = tail[i];
	}
}

PD_NO_BUILTIN void *
_platform_memset(void *b, int c, size_t len)
{
	unsigned char *d = b;
	v16 v;

	if (len >= PD_REP_THRESHOLD && has_erms()) {
		rep_stosb(d, c & 0xff, len);
		return b;
	}
	if (len < 16) {
		for (size_t i = 0; i < len; i++) {
			d[i] = (unsigned char)c;
		}
		return b;
	}
	v = (v16){ 0 } + (unsigned char)c;
	// overlapping tail store replaces a byte loop
	*(v16 *)(d + len - 16) = v;
	for (size_t i = 0; i + 16 <= len; i += 16) {
		*(v16 *)(d + i) = v;
	}
	return b;
}

// a w-byte pattern repeated across 16 bytes
static inline v16
pattern(const void *p, size_t w)
{
	v16 v;

	for (size_t i = 0; i < 16; i++) {
		v[i] = ((const unsigned char *)p)[i % w];
	}
	return v;
}

void
_platform_bzero(void *s, size_t n)
{
	_platform_memset(s, 0, n);
}

void
_platform_memset_pattern4(void *b, const void *pattern4, size_t len)
{
	fill16(b, pattern(pattern4, 4), len);
}

void
_platform_memset_pattern8(void *b, const void *pattern8, size_t len)
{
	fill16(b, pattern(pattern8, 8), len);
}

void
_platform_memset_pattern16(void *b, const void *pattern16, size_t len)
{
	fill16(b, *(const v16 *)pattern16, len);
}

#if VARIANT_STATIC
void *
memmove(void *dst, const void *src, size_t n)
{
	return _platform_memmove(dst, src, n);
}

void *
memcpy(void *dst, const void *src, size_t n)
{
	return _platform_memmove(dst, src, n);
}

void *
memset(void *b, int c, size_t len)
{
	return _platform_memset(b, c, len);
}

void
bzero(void *s, size_t n)
{
	_platform_bzero(s, n);
}

void
__bzero(void *s, size_t n)
{
	_platform_bzero(s, n);
}

void
memset_pattern4(void *b, const void *pattern4, size_t len)
{
	_platform_memset_pattern4(b, pattern4, len);
}

void
memset_pattern8(void *b, const void *pattern8, size_t len)
{
	_platform_memset_pattern8(b, pattern8, len);
}

void
memset_pattern16(void *b, const void *pattern16, size_t len)
{
	_platform_memset_pattern16(b, pattern16, len);
}
#endif

#endif

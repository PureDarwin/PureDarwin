/*
 * Copyright 2002-2021 Haiku, Inc. All rights reserved.
 * Distributed under the terms of the MIT License.
 */
#ifndef _STDLIB_H_
#define _STDLIB_H_


#include <alloca.h>
#include <div_t.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <wchar_t.h>


#define RAND_MAX      0x7fffffff
#define MB_CUR_MAX    (__ctype_get_mb_cur_max())

#define EXIT_SUCCESS  0
#define EXIT_FAILURE  1

/* random data structures (for thread-safe random numbers) */
struct random_data  {
    int *fptr;
    int *rptr;
    int *state;
    int rand_type;
    int rand_deg;
    int rand_sep;
    int *end_ptr;
};


#ifdef  __cplusplus
extern "C" {
#endif

/* memory allocation (see malloc.h for additional defines & prototypes) */
extern void		*calloc(size_t numElements, size_t size);
extern void		free(void *pointer);
extern void		*malloc(size_t size);
extern int		posix_memalign(void **_pointer, size_t alignment, size_t size);
extern void 	*aligned_alloc(size_t alignment, size_t size) _ALIGNED_BY_ARG(1);
extern void		*realloc(void *oldPointer, size_t newSize);
extern void		*reallocarray(void *ptr, size_t nelem, size_t elsize);

/* process termination */
extern void		abort(void) __attribute__((noreturn));
extern int		atexit(void (*func)(void));
extern int		atfork(void (*func)(void));
extern void		exit(int) __attribute__((noreturn));
extern void		_Exit(int) __attribute__((noreturn));

/* misc functions */
extern char		*realpath(const char *path, char *resolved);

extern int		system(const char *command);

extern char		*mktemp(char *name);
extern char		*mkdtemp(char *templat);
extern int		mkstemp(char *templat);
extern int		mkostemp(char *templat, int oflags);

extern char		*ecvt(double value, int digits, int *_decimalPoint, int *_sign);
extern char		*fcvt(double value, int precision, int *_decimalPoint,
					int *_sign);
extern char		*gcvt(double value, int digits, char *buffer);

extern char		*l64a(long n);
extern long		a64l(const char *string);

/* environment variables */
extern char		**environ;
extern int		clearenv(void);
extern char		*getenv(const char *name);
extern int		putenv(char *string);
extern int		setenv(char const *name, char const *value, int rewrite);
extern int		unsetenv(const char *name);

/* ASCII string to number conversion */
extern double			atof(const char *string);
extern int				atoi(const char *string);
extern long				atol(const char *string);
extern long long int	atoll(const char *string);
extern unsigned int 	atoui(const char *string);
extern unsigned long	atoul(const char *string);

extern double			strtod(const char *string, char **end);
extern long double		strtold(const char *string, char **end);
extern float			strtof(const char *string, char **end);
extern long				strtol(const char *string, char **end, int base);
extern unsigned long	strtoul(const char *string, char **end, int base);
extern long long		strtoll(const char *string, char **end, int base);
extern unsigned long long strtoull(const char *string, char **end, int base);

/* random number generation */
extern void		srand(unsigned int seed);
extern int		rand(void);
extern int		random(void);
extern void		srandom(unsigned int seed);
extern int		rand_r(unsigned int *seed);
extern int		random_r(struct random_data *data, int *result);
extern int		srandom_r(unsigned int seed, struct random_data *data);
extern char		*initstate(unsigned int seed, char *state, size_t size);
extern char		*setstate(char *state);
extern int		initstate_r(unsigned int seed, void *stateBuffer,
					size_t stateLength, struct random_data *data);
extern int		setstate_r(void *stateBuffer, struct random_data *data);

extern double	drand48(void);
extern double	erand48(unsigned short int xsubi[3]);
extern long		lrand48(void);
extern long		nrand48(unsigned short int xsubi[3]);
extern long 	mrand48(void);
extern long		jrand48(unsigned short int xsubi[3]);
extern void		srand48(long int seed);
extern unsigned short *seed48(unsigned short int seed16v[3]);
extern void		lcong48(unsigned short int param[7]);

/* search and sort functions */
typedef int (*_compare_function)(const void *, const void *);
typedef int (*_compare_function_qsort_r)(const void*, const void*, void*);

extern void		*bsearch(const void *key, const void *base, size_t numElements,
					size_t sizeOfElement, _compare_function);
extern void		qsort(void *base, size_t numElements, size_t sizeOfElement,
					_compare_function);
extern void		qsort_r(void* base, size_t numElements, size_t sizeOfElement,
					_compare_function_qsort_r, void* cookie);

#ifdef _DEFAULT_SOURCE
extern int		heapsort(void *base, size_t numElements, size_t sizeOfElement,
					_compare_function);
extern int		mergesort(void *base, size_t numElements, size_t sizeOfElement,
					_compare_function);
extern int		radixsort(u_char const **base, int numElements,
					u_char const *table, u_int endByte);
extern int		sradixsort(u_char const **base, int numElements,
					u_char const *table, u_int endByte);
#endif

/* misc math functions */
extern int		abs(int number);
extern long		labs(long number);
extern long long llabs(long long number);

extern div_t	div(int numerator,  int denominator);
extern ldiv_t	ldiv(long numerator, long denominator);
extern lldiv_t	lldiv(long long numerator, long long denominator);

/* wide & multibyte string functions */
extern int		mblen(const char *string, size_t maxSize);
extern int		mbtowc(wchar_t *pwc, const char *string, size_t maxSize);
extern int		wctomb(char *string, wchar_t wchar);
extern size_t	mbstowcs(wchar_t *pwcs, const char *string, size_t maxSize);
extern size_t	wcstombs(char *string, const wchar_t *pwcs, size_t maxSize);

/* crypt */
extern void 	setkey(const char *key);

/* sub-option parsing */
extern int		getsubopt(char **optionp, char * const *keylistp,
					char **valuep);

/* pty functions */
extern int		posix_openpt(int openFlags);
extern int		grantpt(int masterFD);
extern char*	ptsname(int masterFD);
extern int		ptsname_r(int masterFD, char *path, size_t maxSize);
extern int		unlockpt(int masterFD);

/* internal accessor to value for MB_CUR_MAX */
extern unsigned short __ctype_get_mb_cur_max(void);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* _STDLIB_H_ */

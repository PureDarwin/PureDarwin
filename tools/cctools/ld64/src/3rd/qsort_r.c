/*-
 * Copyright (c) 1992, 1993
 *  The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#define I_AM_QSORT_R

#include <sys/cdefs.h>

#include <errno.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#define qsort_r qsort_r_local

#ifndef __FreeBSD__
/* flsl.c */
int
flsl(long mask)
{
    int bit;

    if (mask == 0)
        return (0);
    for (bit = 1; mask != 1; bit++)
        mask = (unsigned long)mask >> 1;
    return (bit);
}

/* fsl.c */
int
fls(int mask)
{
    int bit;
    if (mask == 0)
        return (0);
    for (bit = 1; mask != 1; bit++)
        mask = (unsigned int)mask >> 1;
    return (bit);
}
#endif

/*
 * Swap two areas of size number of bytes.  Although qsort(3) permits random
 * blocks of memory to be sorted, sorting pointers is almost certainly the
 * common case (and, were it not, could easily be made so).  Regardless, it
 * isn't worth optimizing; the SWAP's get sped up by the cache, and pointer
 * arithmetic gets lost in the time required for comparison function calls.
 */
#define SWAP(a, b, count, size, tmp) { \
    count = size; \
    do { \
        tmp = *a; \
        *a++ = *b; \
        *b++ = tmp; \
    } while (--count); \
}

/* Copy one block of size size to another. */
#define COPY(a, b, count, size, tmp1, tmp2) { \
    count = size; \
    tmp1 = a; \
    tmp2 = b; \
    do { \
        *tmp1++ = *tmp2++; \
    } while (--count); \
}

/*
 * Build the list into a heap, where a heap is defined such that for
 * the records K1 ... KN, Kj/2 >= Kj for 1 <= j/2 <= j <= N.
 *
 * There two cases.  If j == nmemb, select largest of Ki and Kj.  If
 * j < nmemb, select largest of Ki, Kj and Kj+1.
 */
#define CREATE(initval, nmemb, par_i, child_i, par, child, size, count, tmp) { \
    for (par_i = initval; (child_i = par_i * 2) <= nmemb; \
        par_i = child_i) { \
        child = base + child_i * size; \
        if (child_i < nmemb && compar(thunk, child, child + size) < 0) { \
            child += size; \
            ++child_i; \
        } \
        par = base + par_i * size; \
        if (compar(thunk, child, par) <= 0) \
            break; \
        SWAP(par, child, count, size, tmp); \
    } \
}

/*
 * Select the top of the heap and 'heapify'.  Since by far the most expensive
 * action is the call to the compar function, a considerable optimization
 * in the average case can be achieved due to the fact that k, the displaced
 * elememt, is ususally quite small, so it would be preferable to first
 * heapify, always maintaining the invariant that the larger child is copied
 * over its parent's record.
 *
 * Then, starting from the *bottom* of the heap, finding k's correct place,
 * again maintianing the invariant.  As a result of the invariant no element
 * is 'lost' when k is assigned its correct place in the heap.
 *
 * The time savings from this optimization are on the order of 15-20% for the
 * average case. See Knuth, Vol. 3, page 158, problem 18.
 *
 * XXX Don't break the #define SELECT line, below.  Reiser cpp gets upset.
 */
#define SELECT(par_i, child_i, nmemb, par, child, size, k, count, tmp1, tmp2) { \
    for (par_i = 1; (child_i = par_i * 2) <= nmemb; par_i = child_i) { \
        child = base + child_i * size; \
        if (child_i < nmemb && compar(thunk, child, child + size) < 0) { \
            child += size; \
            ++child_i; \
        } \
        par = base + par_i * size; \
        COPY(par, child, count, size, tmp1, tmp2); \
    } \
    for (;;) { \
        child_i = par_i; \
        par_i = child_i / 2; \
        child = base + child_i * size; \
        par = base + par_i * size; \
        if (child_i == 1 || compar(thunk, k, par) < 0) { \
            COPY(child, k, count, size, tmp1, tmp2); \
            break; \
        } \
        COPY(child, par, count, size, tmp1, tmp2); \
    } \
}

/*
 * Heapsort -- Knuth, Vol. 3, page 145.  Runs in O (N lg N), both average
 * and worst.  While heapsort is faster than the worst case of quicksort,
 * the BSD quicksort does median selection so that the chance of finding
 * a data set that will trigger the worst case is nonexistent.  Heapsort's
 * only advantage over quicksort is that it requires little additional memory.
 */
__private_extern__ int
__heapsort_r(
    void *vbase,
    size_t nmemb,
    size_t size,
    void *thunk,
    int (*compar)(void *, const void *, const void *)
)
{
    size_t cnt, i, j, l;
    char tmp, *tmp1, *tmp2;
    char *base, *k, *p, *t;

    if (nmemb <= 1)
        return (0);

    if (!size) {
        errno = EINVAL;
        return (-1);
    }

    if ((k = malloc(size)) == NULL)
        return (-1);

    /*
     * Items are numbered from 1 to nmemb, so offset from size bytes
     * below the starting address.
     */
    base = (char *)vbase - size;

    for (l = nmemb / 2 + 1; --l;)
        CREATE(l, nmemb, i, j, t, p, size, cnt, tmp);

    /*
     * For each element of the heap, save the largest element into its
     * final slot, save the displaced element (k), then recreate the
     * heap.
     */
    while (nmemb > 1) {
        COPY(k, base + nmemb * size, cnt, size, tmp1, tmp2);
        COPY(base + nmemb * size, base + size, cnt, size, tmp1, tmp2);
        --nmemb;
        SELECT(i, j, nmemb, t, p, size, k, cnt, tmp1, tmp2);
    }
    free(k);
    return (0);
}

/* qsort.c */

#include <string.h>

#ifdef I_AM_QSORT_R
typedef int      cmp_t(void *, const void *, const void *);
#else
typedef int      cmp_t(const void *, const void *);
#endif
#ifdef I_AM_QSORT_B
static inline char  *med3(char *, char *, char *, cmp_t ^, void *) __attribute__((always_inline));
#else
static inline char  *med3(char *, char *, char *, cmp_t *, void *) __attribute__((always_inline));
#endif
static inline void   swapfunc(char *, char *, int, int) __attribute__((always_inline));

#define min(a, b)   (a) < (b) ? a : b

/*
 * Qsort routine from Bentley & McIlroy's "Engineering a Sort Function".
 */
#define swapcode(TYPE, parmi, parmj, n) {       \
    long i = (n) / sizeof (TYPE);           \
    TYPE *pi = (TYPE *) (parmi);        \
    TYPE *pj = (TYPE *) (parmj);        \
    do {                        \
        TYPE    t = *pi;        \
        *pi++ = *pj;                \
        *pj++ = t;              \
        } while (--i > 0);              \
}

#define SWAPINIT(a, es) swaptype = ((char *)a - (char *)0) % sizeof(long) || \
    es % sizeof(long) ? 2 : es == sizeof(long)? 0 : 1;

static inline void
swapfunc(a, b, n, swaptype)
    char *a, *b;
    int n, swaptype;
{
    if(swaptype <= 1)
        swapcode(long, a, b, n)
    else
        swapcode(char, a, b, n)
}

#define swap(a, b)                  \
    if (swaptype == 0) {                \
        long t = *(long *)(a);          \
        *(long *)(a) = *(long *)(b);        \
        *(long *)(b) = t;           \
    } else                      \
        swapfunc(a, b, es, swaptype)

#define vecswap(a, b, n)    if ((n) > 0) swapfunc(a, b, n, swaptype)

#ifdef I_AM_QSORT_R
#define CMP(t, x, y) (cmp((t), (x), (y)))
#else
#define CMP(t, x, y) (cmp((x), (y)))
#endif

static inline char *
med3(char *a, char *b, char *c,
#ifdef I_AM_QSORT_B
cmp_t ^cmp,
#else
cmp_t *cmp,
#endif
void *thunk
#ifndef I_AM_QSORT_R
__unused
#endif
)
{
    return CMP(thunk, a, b) < 0 ?
           (CMP(thunk, b, c) < 0 ? b : (CMP(thunk, a, c) < 0 ? c : a ))
              :(CMP(thunk, b, c) > 0 ? b : (CMP(thunk, a, c) < 0 ? a : c ));
}

#ifdef __LP64__
#define DEPTH(x)    (2 * (flsl((long)(x)) - 1))
#else /* !__LP64__ */
#define DEPTH(x)    (2 * (fls((int)(x)) - 1))
#endif /* __LP64__ */

#ifdef I_AM_QSORT_R
int __heapsort_r(void *, size_t, size_t, void *, int (*)(void *, const void *, const void *));
#endif

static void
_qsort(void *a, size_t n, size_t es,
#ifdef I_AM_QSORT_R
void *thunk,
#else
#define thunk   NULL
#endif
#ifdef I_AM_QSORT_B
cmp_t ^cmp,
#else
cmp_t *cmp,
#endif
int depth_limit)
{
    char *pa, *pb, *pc, *pd, *pl, *pm, *pn;
    size_t d, r;
    int cmp_result;
    int swaptype, swap_cnt;

loop:
    if (depth_limit-- <= 0) {
#ifdef I_AM_QSORT_B
        heapsort_b(a, n, es, cmp);
#elif defined(I_AM_QSORT_R)
        __heapsort_r(a, n, es, thunk, cmp);
#else
        heapsort(a, n, es, cmp);
#endif
        return;
    }
    SWAPINIT(a, es);
    swap_cnt = 0;
    if (n < 7) {
        for (pm = (char *)a + es; pm < (char *)a + n * es; pm += es)
            for (pl = pm; 
                 pl > (char *)a && CMP(thunk, pl - es, pl) > 0;
                 pl -= es)
                swap(pl, pl - es);
        return;
    }
    pm = (char *)a + (n / 2) * es;
    if (n > 7) {
        pl = a;
        pn = (char *)a + (n - 1) * es;
        if (n > 40) {
            d = (n / 8) * es;
            pl = med3(pl, pl + d, pl + 2 * d, cmp, thunk);
            pm = med3(pm - d, pm, pm + d, cmp, thunk);
            pn = med3(pn - 2 * d, pn - d, pn, cmp, thunk);
        }
        pm = med3(pl, pm, pn, cmp, thunk);
    }
    swap(a, pm);
    pa = pb = (char *)a + es;

    pc = pd = (char *)a + (n - 1) * es;
    for (;;) {
        while (pb <= pc && (cmp_result = CMP(thunk, pb, a)) <= 0) {
            if (cmp_result == 0) {
                swap_cnt = 1;
                swap(pa, pb);
                pa += es;
            }
            pb += es;
        }
        while (pb <= pc && (cmp_result = CMP(thunk, pc, a)) >= 0) {
            if (cmp_result == 0) {
                swap_cnt = 1;
                swap(pc, pd);
                pd -= es;
            }
            pc -= es;
        }
        if (pb > pc)
            break;
        swap(pb, pc);
        swap_cnt = 1;
        pb += es;
        pc -= es;
    }

    pn = (char *)a + n * es;
    r = min(pa - (char *)a, pb - pa);
    vecswap(a, pb - r, r);
    r = min(pd - pc, pn - pd - es);
    vecswap(pb, pn - r, r);

    if (swap_cnt == 0) {  /* Switch to insertion sort */
        r = 1 + n / 4; /* n >= 7, so r >= 2 */
        for (pm = (char *)a + es; pm < (char *)a + n * es; pm += es)
            for (pl = pm; 
                 pl > (char *)a && CMP(thunk, pl - es, pl) > 0;
                 pl -= es) {
                swap(pl, pl - es);
                if (++swap_cnt > r) goto nevermind;
            }
        return;
    }

nevermind:
    if ((r = pb - pa) > es)
#ifdef I_AM_QSORT_R
        _qsort(a, r / es, es, thunk, cmp, depth_limit);
#else
        _qsort(a, r / es, es, cmp, depth_limit);
#endif
    if ((r = pd - pc) > es) {
        /* Iterate rather than recurse to save stack space */
        a = pn - r;
        n = r / es;
        goto loop;
    }
/*      qsort(pn - r, r / es, es, cmp);*/
}

void
#ifdef I_AM_QSORT_R
qsort_r(void *a, size_t n, size_t es, void *thunk, cmp_t *cmp)
#elif defined(I_AM_QSORT_B)
qsort_b(void *a, size_t n, size_t es, cmp_t ^cmp)
#else
qsort(void *a, size_t n, size_t es, cmp_t *cmp)
#endif
{
    _qsort(a, n, es,
#ifdef I_AM_QSORT_R
        thunk,
#endif
        cmp, DEPTH(n));
}



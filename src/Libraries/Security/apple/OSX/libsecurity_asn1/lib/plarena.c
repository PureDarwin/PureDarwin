/*
 * Copyright (c) 2003-2006,2008,2010-2012 Apple Inc. All Rights Reserved.
 * 
 * @APPLE_LICENSE_HEADER_START@
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
/* 
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is the Netscape Portable Runtime (NSPR).
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are 
 * Copyright (C) 1998-2000 Netscape Communications Corporation.  All
 * Rights Reserved.
 * 
 * Contributor(s):
 * 
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU General Public License Version 2 or later (the
 * "GPL"), in which case the provisions of the GPL are applicable 
 * instead of those above.  If you wish to allow use of your 
 * version of this file only under the terms of the GPL and not to
 * allow others to use your version of this file under the MPL,
 * indicate your decision by deleting the provisions above and
 * replace them with the notice and other provisions required by
 * the GPL.  If you do not delete the provisions above, a recipient
 * may use your version of this file under either the MPL or the
 * GPL.
 */

/*
 * Lifetime-based fast allocation, inspired by much prior art, including
 * "Fast Allocation and Deallocation of Memory Based on Object Lifetimes"
 * David R. Hanson, Software -- Practice and Experience, Vol. 20(1).
 */
#include <stdlib.h>
#include <string.h>
#include "plarena.h"
#include "prmem.h"
#include "prbit.h"
#include "prlog.h"
#include "prinit.h"

#ifdef PL_ARENAMETER
static PLArenaStats *arena_stats_list;

#define COUNT(pool,what)  (pool)->stats.what++
#else
#define COUNT(pool,what)  /* nothing */
#endif

#define PL_ARENA_DEFAULT_ALIGN  sizeof(double)

PR_IMPLEMENT(void) PL_InitArenaPool(
    PLArenaPool *pool, const char *name, PRUint32 size, PRUint32 align)
{
#if !defined (__GNUC__)
#pragma unused (name)
#endif

    if (align == 0)
        align = PL_ARENA_DEFAULT_ALIGN;
    pool->mask = PR_BITMASK(PR_CeilingLog2(align));
    pool->first.next = NULL;
    pool->first.base = pool->first.avail = pool->first.limit =
        (PRUword)PL_ARENA_ALIGN(pool, &pool->first + 1);
    pool->current = &pool->first;
    pool->arenasize = size;                                  
#ifdef PL_ARENAMETER
    memset(&pool->stats, 0, sizeof pool->stats);
    pool->stats.name = strdup(name);
    pool->stats.next = arena_stats_list;
    arena_stats_list = &pool->stats;
#endif
}

#if __APPLE__
#define MAX_SIZE (PR_UINT32_MAX >> 1)
#endif

/*
** PL_ArenaAllocate() -- allocate space from an arena pool
** 
** Description: PL_ArenaAllocate() allocates space from an arena
** pool. 
**
** First, try to satisfy the request from arenas starting at
** pool->current. Then try to allocate a new arena from the heap.
**
** Returns: pointer to allocated space or NULL
** 
** Notes: The original implementation had some difficult to
** solve bugs; the code was difficult to read. Sometimes it's
** just easier to rewrite it. I did that. larryh.
**
** See also: bugzilla: 45343.
**
*/

PR_IMPLEMENT(void *) PL_ArenaAllocate(PLArenaPool *pool, PRUint32 nb)
{
    PLArena *a;   
    char *rp;     /* returned pointer */
    PRUint32 nbOld;

    PR_ASSERT((nb & pool->mask) == 0);
    nbOld = nb;
#ifdef __APPLE__
    nb = PL_ARENA_ALIGN(pool, nb); /* force alignment, cast is useless/causes warning. */
#else
    nb = (PRUword)PL_ARENA_ALIGN(pool, nb); /* force alignment */
#endif
    /* ensure that alignment didn't cause overflow */
    if (nb < nbOld)
        return NULL;

    /* attempt to allocate from arenas at pool->current */
    {
        a = pool->current;
        do {
            if ( nb <= a->limit - a->avail )  {
                pool->current = a;
                rp = (char *)a->avail;
                a->avail += nb;
                return rp;
            }
        } while( NULL != (a = a->next) );
    }

    /* attempt to allocate from the heap */ 
    {  
        PRUint32 sz = PR_MAX(pool->arenasize, nb);
        if (PR_UINT32_MAX - sz < sizeof *a + pool->mask) {
            a = NULL;
        } else {
            sz += sizeof *a + pool->mask;  /* header and alignment slop */
            a = (PLArena*)PR_MALLOC(sz);
        }
#ifdef __APPLE__
        // Check for integer overflow on a->avail += nb
        PRUword a_avail_tmp=(PRUword)PL_ARENA_ALIGN(pool, a + 1);
        if (a_avail_tmp + nb < a_avail_tmp)
        {
            PR_FREEIF(a); // Set a back to NULL
        }
#endif
        if ( NULL != a )  {
            a->limit = (PRUword)a + sz;
#ifdef __APPLE__
            a->base = a->avail = a_avail_tmp;
#else
            a->base = a->avail = (PRUword)PL_ARENA_ALIGN(pool, a + 1);
#endif
            rp = (char *)a->avail;
            a->avail += nb;
            /* the newly allocated arena is linked after pool->current 
            *  and becomes pool->current */
            a->next = pool->current->next;
            pool->current->next = a;
            pool->current = a;
            if ( NULL == pool->first.next )
                pool->first.next = a;
            PL_COUNT_ARENA(pool,++);
            COUNT(pool, nmallocs);
            return(rp);
        }
    }

    /* we got to here, and there's no memory to allocate */
    return(NULL);
} /* --- end PL_ArenaAllocate() --- */

/*
 * Grow, a.k.a. realloc. The PL_ARENA_GROW macro has already handled
 * the possible grow-in-place action in which the current PLArena is the 
 * source of the incoming pointer, and there is room in that arena for 
 * the requested size. 
 */
PR_IMPLEMENT(void *) PL_ArenaGrow(
    PLArenaPool *pool, void *p, PRUint32 origSize, PRUint32 incr)
{
    void *newp;
	PLArena *thisArena;
	PLArena *lastArena;
	PRUint32 origAlignSize;		// bytes currently reserved for caller
	PRUint32 newSize;			// bytes actually mallocd here

    // Enforce maximal size before any potential implicit truncation
    if (origSize>=MAX_SIZE || incr>=MAX_SIZE) {
        return NULL;
    }
	origAlignSize = PL_ARENA_ALIGN(pool, origSize);
    if (origAlignSize>=MAX_SIZE) {
        return NULL;
    }

    /* expand at least by 2x */
    newSize = PR_MAX(origAlignSize+incr, 2*origAlignSize);
    newSize = PL_ARENA_ALIGN(pool, newSize);

    if (newSize>=MAX_SIZE) {
        return NULL;
    }

    PL_ARENA_ALLOCATE(newp, pool, newSize);
    if (newp == NULL) {
        return NULL;
    }
	/* 
	 * Trim back the memory we just allocated to the amount our caller really
	 * needs, leaving the remainder for grow-in-place on subsequent calls
	 * to PL_ARENA_GROW.
	 */
	PRUint32 newAlignSize = PL_ARENA_ALIGN(pool, origSize+incr);
	PR_ASSERT(pool->current->avail == ((PRUword)newp + newSize));
	pool->current->avail = (PRUword)newp + newAlignSize;
	PR_ASSERT(pool->current->avail <= pool->current->limit);
	
	/* "realloc" */
	memcpy(newp, p, origSize);

	/*
	 * Free old memory only if it's the entire outstanding allocated 
	 * memory associated with one of our known PLArenas. 
	 */
	lastArena = &pool->first;			/* pool->first always empty */
	thisArena = lastArena->next;		/* so, start here */
	
	PRUword origPtr = (PRUword)p;
	while(thisArena != NULL) {
		if(origPtr == thisArena->base) {
			if((origPtr + origAlignSize) == thisArena->avail) {
				/* unlink */
				lastArena->next = thisArena->next;
				
				/* and free */
				PL_CLEAR_ARENA(thisArena);
				PL_COUNT_ARENA(pool,--);
				PR_DELETE(thisArena);
				break;
			}
		}
		lastArena = thisArena;
		thisArena = thisArena->next;
	}
	/* 
	 * Note: inability to free is not an error; it just causes a temporary leak
	 * of the old buffer (until the arena pool is freed, of course).
	 */
    return newp;
}

static void ClearArenaList(PLArena *a, PRInt32 pattern)
{

    for (; a; a = a->next) {
        PR_ASSERT(a->base <= a->avail && a->avail <= a->limit);
        a->avail = a->base;
        PL_CLEAR_UNUSED_PATTERN(a, pattern);
    }
}

PR_IMPLEMENT(void) PL_ClearArenaPool(PLArenaPool *pool, PRInt32 pattern)
{
    ClearArenaList(pool->first.next, pattern);
}

/*
 * Free tail arenas linked after head, which may not be the true list head.
 * Reset pool->current to point to head in case it pointed at a tail arena.
 */
static void FreeArenaList(PLArenaPool *pool, PLArena *head, PRBool reallyFree)
{
    PLArena **ap, *a;

    ap = &head->next;
    a = *ap;
    if (!a) {
        return;
    }

	do {
		*ap = a->next;
		PL_CLEAR_ARENA(a);
		PL_COUNT_ARENA(pool,--);
		PR_DELETE(a);
	} while ((a = *ap) != 0);

    pool->current = head;
}

PR_IMPLEMENT(void) PL_ArenaRelease(PLArenaPool *pool, char *mark)
{
	#if ARENA_MARK_ENABLE
    PLArena *a;

    for (a = pool->first.next; a; a = a->next) {
        if (PR_UPTRDIFF(mark, a->base) < PR_UPTRDIFF(a->avail, a->base)) {
            a->avail = (PRUword)PL_ARENA_ALIGN(pool, mark);
            FreeArenaList(pool, a, PR_FALSE);
            return;
        }
    }
	#endif	/* ARENA_MARK_ENABLE */
}

PR_IMPLEMENT(void) PL_FreeArenaPool(PLArenaPool *pool)
{
    FreeArenaList(pool, &pool->first, PR_FALSE);
    COUNT(pool, ndeallocs);
}

PR_IMPLEMENT(void) PL_FinishArenaPool(PLArenaPool *pool)
{
    FreeArenaList(pool, &pool->first, PR_TRUE);
#ifdef PL_ARENAMETER
    {
        PLArenaStats *stats, **statsp;

        if (pool->stats.name)
            PR_DELETE(pool->stats.name);
        for (statsp = &arena_stats_list; (stats = *statsp) != 0;
             statsp = &stats->next) {
            if (stats == &pool->stats) {
                *statsp = stats->next;
                return;
            }
        }
    }
#endif
}

PR_IMPLEMENT(void) PL_CompactArenaPool(PLArenaPool *ap)
{
}

PR_IMPLEMENT(void) PL_ArenaFinish(void)
{
}

#ifdef PL_ARENAMETER
PR_IMPLEMENT(void) PL_ArenaCountAllocation(PLArenaPool *pool, PRUint32 nb)
{
    pool->stats.nallocs++;
    pool->stats.nbytes += nb;
    if (nb > pool->stats.maxalloc)
        pool->stats.maxalloc = nb;
    pool->stats.variance += nb * nb;
}

PR_IMPLEMENT(void) PL_ArenaCountInplaceGrowth(
    PLArenaPool *pool, PRUint32 size, PRUint32 incr)
{
    pool->stats.ninplace++;
}

PR_IMPLEMENT(void) PL_ArenaCountGrowth(
    PLArenaPool *pool, PRUint32 size, PRUint32 incr)
{
    pool->stats.ngrows++;
    pool->stats.nbytes += incr;
    pool->stats.variance -= size * size;
    size += incr;
    if (size > pool->stats.maxalloc)
        pool->stats.maxalloc = size;
    pool->stats.variance += size * size;
}

PR_IMPLEMENT(void) PL_ArenaCountRelease(PLArenaPool *pool, char *mark)
{
    pool->stats.nreleases++;
}

PR_IMPLEMENT(void) PL_ArenaCountRetract(PLArenaPool *pool, char *mark)
{
    pool->stats.nfastrels++;
}

#include <math.h>
#include <stdio.h>

PR_IMPLEMENT(void) PL_DumpArenaStats(FILE *fp)
{
    PLArenaStats *stats;
    double mean, variance;

    for (stats = arena_stats_list; stats; stats = stats->next) {
        if (stats->nallocs != 0) {
            mean = (double)stats->nbytes / stats->nallocs;
            variance = fabs(stats->variance / stats->nallocs - mean * mean);
        } else {
            mean = variance = 0;
        }

        fprintf(fp, "\n%s allocation statistics:\n", stats->name);
        fprintf(fp, "              number of arenas: %u\n", stats->narenas);
        fprintf(fp, "         number of allocations: %u\n", stats->nallocs);
        fprintf(fp, " number of free arena reclaims: %u\n", stats->nreclaims);
        fprintf(fp, "        number of malloc calls: %u\n", stats->nmallocs);
        fprintf(fp, "       number of deallocations: %u\n", stats->ndeallocs);
        fprintf(fp, "  number of allocation growths: %u\n", stats->ngrows);
        fprintf(fp, "    number of in-place growths: %u\n", stats->ninplace);
        fprintf(fp, "number of released allocations: %u\n", stats->nreleases);
        fprintf(fp, "       number of fast releases: %u\n", stats->nfastrels);
        fprintf(fp, "         total bytes allocated: %u\n", stats->nbytes);
        fprintf(fp, "          mean allocation size: %g\n", mean);
        fprintf(fp, "            standard deviation: %g\n", sqrt(variance));
        fprintf(fp, "       maximum allocation size: %u\n", stats->maxalloc);
    }
}
#endif /* PL_ARENAMETER */

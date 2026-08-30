/*
 * Copyright (c) 1998-2022 Apple Inc. All rights reserved.
 *
 * @APPLE_OSREFERENCE_LICENSE_HEADER_START@
 *
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. The rights granted to you under the License
 * may not be used to create, or enable the creation or redistribution of,
 * unlawful or unlicensed copies of an Apple operating system, or to
 * circumvent, violate, or enable the circumvention or violation of, any
 * terms of an Apple operating system software license agreement.
 *
 * Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this file.
 *
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 *
 * @APPLE_OSREFERENCE_LICENSE_HEADER_END@
 */
/* Copyright (c) 1995 NeXT Computer, Inc. All Rights Reserved */
/*
 * Copyright (c) 1982, 1986, 1988, 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
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
 *
 *	@(#)uipc_mbuf.c	8.2 (Berkeley) 1/4/94
 */
/*
 * NOTICE: This file was modified by SPARTA, Inc. in 2005 to introduce
 * support for mandatory and extensible security protections.  This notice
 * is included in support of clause 2.2 (b) of the Apple Public License,
 * Version 2.0.
 */

#include <ptrauth.h>

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/kernel.h>
#include <sys/sysctl.h>
#include <sys/syslog.h>
#include <sys/protosw.h>
#include <sys/domain.h>
#include <sys/queue.h>
#include <sys/proc.h>
#include <sys/filedesc.h>
#include <sys/file_internal.h>

#include <vm/vm_kern_xnu.h>

#include <dev/random/randomdev.h>

#include <kern/kern_types.h>
#include <kern/simple_lock.h>
#include <kern/queue.h>
#include <kern/sched_prim.h>
#include <kern/backtrace.h>
#include <kern/percpu.h>
#include <kern/zalloc.h>

#include <libkern/OSDebug.h>
#include <libkern/libkern.h>

#include <os/log.h>
#include <os/ptrtools.h>

#include <IOKit/IOMapper.h>

#include <machine/limits.h>
#include <machine/machine_routines.h>

#include <sys/mcache.h>

#include <net/droptap.h>
#include <netinet/mptcp_var.h>
#include <netinet/tcp_var.h>

#define DUMP_BUF_CHK() {        \
	clen -= k;              \
	if (clen < 1)           \
	        goto done;      \
	c += k;                 \
}

#if INET
static int
dump_tcp_reass_qlen(char *str, int str_len)
{
	char *c = str;
	int k, clen = str_len;

	if (tcp_reass_total_qlen != 0) {
		k = scnprintf(c, clen, "\ntcp reass qlen %d\n", tcp_reass_total_qlen);
		DUMP_BUF_CHK();
	}

done:
	return str_len - clen;
}
#endif /* INET */

#if MPTCP
static int
dump_mptcp_reass_qlen(char *str, int str_len)
{
	char *c = str;
	int k, clen = str_len;

	if (mptcp_reass_total_qlen != 0) {
		k = scnprintf(c, clen, "\nmptcp reass qlen %d\n", mptcp_reass_total_qlen);
		DUMP_BUF_CHK();
	}

done:
	return str_len - clen;
}
#endif /* MPTCP */

#if NETWORKING
extern int dlil_dump_top_if_qlen(char *__counted_by(str_len), int str_len);
#endif /* NETWORKING */

/*
 * MBUF IMPLEMENTATION NOTES.
 *
 * There is a total of 5 per-CPU caches:
 *
 * MC_MBUF:
 *	This is a cache of rudimentary objects of _MSIZE in size; each
 *	object represents an mbuf structure.  This cache preserves only
 *	the m_type field of the mbuf during its transactions.
 *
 * MC_CL:
 *	This is a cache of rudimentary objects of MCLBYTES in size; each
 *	object represents a mcluster structure.  This cache does not
 *	preserve the contents of the objects during its transactions.
 *
 * MC_BIGCL:
 *	This is a cache of rudimentary objects of MBIGCLBYTES in size; each
 *	object represents a mbigcluster structure.  This cache does not
 *	preserve the contents of the objects during its transaction.
 *
 * MC_MBUF_CL:
 *	This is a cache of mbufs each having a cluster attached to it.
 *	It is backed by MC_MBUF and MC_CL rudimentary caches.  Several
 *	fields of the mbuf related to the external cluster are preserved
 *	during transactions.
 *
 * MC_MBUF_BIGCL:
 *	This is a cache of mbufs each having a big cluster attached to it.
 *	It is backed by MC_MBUF and MC_BIGCL rudimentary caches.  Several
 *	fields of the mbuf related to the external cluster are preserved
 *	during transactions.
 *
 * OBJECT ALLOCATION:
 *
 * Allocation requests are handled first at the per-CPU (mcache) layer
 * before falling back to the slab layer.  Performance is optimal when
 * the request is satisfied at the CPU layer because global data/lock
 * never gets accessed.  When the slab layer is entered for allocation,
 * the slab freelist will be checked first for available objects before
 * the VM backing store is invoked.  Slab layer operations are serialized
 * for all of the caches as the mbuf global lock is held most of the time.
 * Allocation paths are different depending on the class of objects:
 *
 * a. Rudimentary object:
 *
 *	{ m_get_common(), m_clattach(), m_mclget(),
 *	  m_mclalloc(), m_bigalloc(), m_copym_with_hdrs(),
 *	  composite object allocation }
 *			|	^
 *			|	|
 *			|	+-----------------------+
 *			v				|
 *	   mcache_alloc/mcache_alloc_ext()	mbuf_slab_audit()
 *			|				^
 *			v				|
 *		   [CPU cache] ------->	(found?) -------+
 *			|				|
 *			v				|
 *		 mbuf_slab_alloc()			|
 *			|				|
 *			v				|
 *	+---------> [freelist] ------->	(found?) -------+
 *	|		|
 *	|		v
 *	|	    m_clalloc()
 *	|		|
 *	|		v
 *	+---<<---- kmem_mb_alloc()
 *
 * b. Composite object:
 *
 *	{ m_getpackets_internal(), m_allocpacket_internal() }
 *			|	^
 *			|	|
 *			|	+------	(done) ---------+
 *			v				|
 *	   mcache_alloc/mcache_alloc_ext()	mbuf_cslab_audit()
 *			|				^
 *			v				|
 *		   [CPU cache] ------->	(found?) -------+
 *			|				|
 *			v				|
 *		 mbuf_cslab_alloc()			|
 *			|				|
 *			v				|
 *		    [freelist] ------->	(found?) -------+
 *			|				|
 *			v				|
 *		(rudimentary object)			|
 *	   mcache_alloc/mcache_alloc_ext() ------>>-----+
 *
 * Auditing notes: If auditing is enabled, buffers will be subjected to
 * integrity checks by the audit routine.  This is done by verifying their
 * contents against DEADBEEF (free) pattern before returning them to caller.
 * As part of this step, the routine will also record the transaction and
 * pattern-fill the buffers with BADDCAFE (uninitialized) pattern.  It will
 * also restore any constructed data structure fields if necessary.
 *
 * OBJECT DEALLOCATION:
 *
 * Freeing an object simply involves placing it into the CPU cache; this
 * pollutes the cache to benefit subsequent allocations.  The slab layer
 * will only be entered if the object is to be purged out of the cache.
 * During normal operations, this happens only when the CPU layer resizes
 * its bucket while it's adjusting to the allocation load.  Deallocation
 * paths are different depending on the class of objects:
 *
 * a. Rudimentary object:
 *
 *	{ m_free(), m_freem_list(), composite object deallocation }
 *			|	^
 *			|	|
 *			|	+------	(done) ---------+
 *			v				|
 *	   mcache_free/mcache_free_ext()		|
 *			|				|
 *			v				|
 *		mbuf_slab_audit()			|
 *			|				|
 *			v				|
 *		   [CPU cache] ---> (not purging?) -----+
 *			|				|
 *			v				|
 *		 mbuf_slab_free()			|
 *			|				|
 *			v				|
 *		    [freelist] ----------->>------------+
 *	 (objects get purged to VM only on demand)
 *
 * b. Composite object:
 *
 *	{ m_free(), m_freem_list() }
 *			|	^
 *			|	|
 *			|	+------	(done) ---------+
 *			v				|
 *	   mcache_free/mcache_free_ext()		|
 *			|				|
 *			v				|
 *		mbuf_cslab_audit()			|
 *			|				|
 *			v				|
 *		   [CPU cache] ---> (not purging?) -----+
 *			|				|
 *			v				|
 *		 mbuf_cslab_free()			|
 *			|				|
 *			v				|
 *		    [freelist] ---> (not purging?) -----+
 *			|				|
 *			v				|
 *		(rudimentary object)			|
 *	   mcache_free/mcache_free_ext() ------->>------+
 *
 * Auditing notes: If auditing is enabled, the audit routine will save
 * any constructed data structure fields (if necessary) before filling the
 * contents of the buffers with DEADBEEF (free) pattern and recording the
 * transaction.  Buffers that are freed (whether at CPU or slab layer) are
 * expected to contain the free pattern.
 *
 * DEBUGGING:
 *
 * Debugging can be enabled by adding "mbuf_debug=0x3" to boot-args; this
 * translates to the mcache flags (MCF_VERIFY | MCF_AUDIT).  Additionally,
 * the CPU layer cache can be disabled by setting the MCF_NOCPUCACHE flag,
 * i.e. modify the boot argument parameter to "mbuf_debug=0x13".  Leak
 * detection may also be disabled by setting the MCF_NOLEAKLOG flag, e.g.
 * "mbuf_debug=0x113".  Note that debugging consumes more CPU and memory.
 *
 * Each object is associated with exactly one mcache_audit_t structure that
 * contains the information related to its last buffer transaction.  Given
 * an address of an object, the audit structure can be retrieved by finding
 * the position of the object relevant to the base address of the cluster:
 *
 *	+------------+			+=============+
 *	| mbuf addr  |			| mclaudit[i] |
 *	+------------+			+=============+
 *	      |				| cl_audit[0] |
 *	i = MTOBG(addr)			+-------------+
 *	      |			+----->	| cl_audit[1] | -----> mcache_audit_t
 *	b = BGTOM(i)		|	+-------------+
 *	      |			|	|     ...     |
 *	x = MCLIDX(b, addr)	|	+-------------+
 *	      |			|	| cl_audit[7] |
 *	      +-----------------+	+-------------+
 *		 (e.g. x == 1)
 *
 * The mclaudit[] array is allocated at initialization time, but its contents
 * get populated when the corresponding cluster is created.  Because a page
 * can be turned into NMBPG number of mbufs, we preserve enough space for the
 * mbufs so that there is a 1-to-1 mapping between them.  A page that never
 * gets (or has not yet) turned into mbufs will use only cl_audit[0] with the
 * remaining entries unused.  For 16KB cluster, only one entry from the first
 * page is allocated and used for the entire object.
 */

extern ppnum_t pmap_find_phys(pmap_t pmap, addr64_t va);
extern vm_map_t mb_map;         /* special map */

static uint32_t mb_kmem_contig_failed;
static uint32_t mb_kmem_failed;
static uint32_t mb_kmem_one_failed;
/* Timestamp of allocation failures. */
static uint64_t mb_kmem_contig_failed_ts;
static uint64_t mb_kmem_failed_ts;
static uint64_t mb_kmem_one_failed_ts;
static uint64_t mb_kmem_contig_failed_size;
static uint64_t mb_kmem_failed_size;
static uint32_t mb_kmem_stats[6];

/* Back-end (common) layer */
static uint64_t mb_expand_cnt;
static uint64_t mb_expand_cl_cnt;
static uint64_t mb_expand_cl_total;
static uint64_t mb_expand_bigcl_cnt;
static uint64_t mb_expand_bigcl_total;
static uint64_t mb_expand_16kcl_cnt;
static uint64_t mb_expand_16kcl_total;
static boolean_t mbuf_worker_needs_wakeup; /* wait channel for mbuf worker */
static uint32_t mbuf_worker_run_cnt;
static uint64_t mbuf_worker_last_runtime;
static uint64_t mbuf_drain_last_runtime;
static int mbuf_worker_ready;   /* worker thread is runnable */
static unsigned int ncpu;                /* number of CPUs */
static ppnum_t *mcl_paddr;      /* Array of cluster physical addresses */
static ppnum_t mcl_pages;       /* Size of array (# physical pages) */
static ppnum_t mcl_paddr_base;  /* Handle returned by IOMapper::iovmAlloc() */
static mcache_t *ref_cache;     /* Cache of cluster reference & flags */
static mcache_t *mcl_audit_con_cache; /* Audit contents cache */
unsigned int mbuf_debug; /* patchable mbuf mcache flags */
static unsigned int mb_normalized; /* number of packets "normalized" */

#define MB_GROWTH_AGGRESSIVE    1       /* Threshold: 1/2 of total */
#define MB_GROWTH_NORMAL        2       /* Threshold: 3/4 of total */

#define MBUF_CLASS_VALID(c) \
	((int)(c) >= MBUF_CLASS_MIN && (int)(c) <= MBUF_CLASS_MAX)

/*
 * mbuf specific mcache allocation request flags.
 */
#define MCR_COMP        MCR_USR1 /* for MC_MBUF_{CL,BIGCL,16KCL} caches */

/*
 * Per-cluster slab structure.
 *
 * A slab is a cluster control structure that contains one or more object
 * chunks; the available chunks are chained in the slab's freelist (sl_head).
 * Each time a chunk is taken out of the slab, the slab's reference count
 * gets incremented.  When all chunks have been taken out, the empty slab
 * gets removed (SLF_DETACHED) from the class's slab list.  A chunk that is
 * returned to a slab causes the slab's reference count to be decremented;
 * it also causes the slab to be reinserted back to class's slab list, if
 * it's not already done.
 *
 * Compartmentalizing of the object chunks into slabs allows us to easily
 * merge one or more slabs together when the adjacent slabs are idle, as
 * well as to convert or move a slab from one class to another; e.g. the
 * mbuf cluster slab can be converted to a regular cluster slab when all
 * mbufs in the slab have been freed.
 *
 * A slab may also span across multiple clusters for chunks larger than
 * a cluster's size.  In this case, only the slab of the first cluster is
 * used.  The rest of the slabs are marked with SLF_PARTIAL to indicate
 * that they are part of the larger slab.
 *
 * Each slab controls a page of memory.
 */
typedef struct mcl_slab {
	struct mcl_slab *sl_next;       /* neighboring slab */
	u_int8_t        sl_class;       /* controlling mbuf class */
	int8_t          sl_refcnt;      /* outstanding allocations */
	int8_t          sl_chunks;      /* chunks (bufs) in this slab */
	u_int16_t       sl_flags;       /* slab flags (see below) */
	u_int16_t       sl_len;         /* slab length */
	void            *sl_base;       /* base of allocated memory */
	void            *sl_head;       /* first free buffer */
	TAILQ_ENTRY(mcl_slab) sl_link;  /* next/prev slab on freelist */
} mcl_slab_t;

#define SLF_MAPPED      0x0001          /* backed by a mapped page */
#define SLF_PARTIAL     0x0002          /* part of another slab */
#define SLF_DETACHED    0x0004          /* not in slab freelist */

/*
 * The array of slabs are broken into groups of arrays per 1MB of kernel
 * memory to reduce the footprint.  Each group is allocated on demand
 * whenever a new piece of memory mapped in from the VM crosses the 1MB
 * boundary.
 */
#define NSLABSPMB       ((1 << MBSHIFT) >> PAGE_SHIFT)

typedef struct mcl_slabg {
	mcl_slab_t      *slg_slab;      /* group of slabs */
} mcl_slabg_t;

/*
 * Number of slabs needed to control a 16KB cluster object.
 */
#define NSLABSP16KB     (M16KCLBYTES >> PAGE_SHIFT)

/*
 * Per-cluster audit structure.
 */
typedef struct {
	mcache_audit_t  **cl_audit;     /* array of audits */
} mcl_audit_t;

typedef struct {
	struct thread   *msa_thread;    /* thread doing transaction */
	struct thread   *msa_pthread;   /* previous transaction thread */
	uint32_t        msa_tstamp;     /* transaction timestamp (ms) */
	uint32_t        msa_ptstamp;    /* prev transaction timestamp (ms) */
	uint16_t        msa_depth;      /* pc stack depth */
	uint16_t        msa_pdepth;     /* previous transaction pc stack */
	void            *msa_stack[MCACHE_STACK_DEPTH];
	void            *msa_pstack[MCACHE_STACK_DEPTH];
} mcl_scratch_audit_t;

typedef struct {
	/*
	 * Size of data from the beginning of an mbuf that covers m_hdr,
	 * pkthdr and m_ext structures.  If auditing is enabled, we allocate
	 * a shadow mbuf structure of this size inside each audit structure,
	 * and the contents of the real mbuf gets copied into it when the mbuf
	 * is freed.  This allows us to pattern-fill the mbuf for integrity
	 * check, and to preserve any constructed mbuf fields (e.g. mbuf +
	 * cluster cache case).  Note that we don't save the contents of
	 * clusters when they are freed; we simply pattern-fill them.
	 */
	u_int8_t                sc_mbuf[(_MSIZE - _MHLEN) + sizeof(_m_ext_t)];
	mcl_scratch_audit_t     sc_scratch __attribute__((aligned(8)));
} mcl_saved_contents_t;

#define AUDIT_CONTENTS_SIZE     (sizeof (mcl_saved_contents_t))

#define MCA_SAVED_MBUF_PTR(_mca)                                        \
	((struct mbuf *)(void *)((mcl_saved_contents_t *)               \
	(_mca)->mca_contents)->sc_mbuf)
#define MCA_SAVED_MBUF_SIZE                                             \
	(sizeof (((mcl_saved_contents_t *)0)->sc_mbuf))
#define MCA_SAVED_SCRATCH_PTR(_mca)                                     \
	(&((mcl_saved_contents_t *)(_mca)->mca_contents)->sc_scratch)

/*
 * mbuf specific mcache audit flags
 */
#define MB_INUSE        0x01    /* object has not been returned to slab */
#define MB_COMP_INUSE   0x02    /* object has not been returned to cslab */
#define MB_SCVALID      0x04    /* object has valid saved contents */

/*
 * Each of the following two arrays hold up to nmbclusters elements.
 */
static mcl_audit_t *mclaudit;   /* array of cluster audit information */
static unsigned int maxclaudit; /* max # of entries in audit table */
static mcl_slabg_t **slabstbl;  /* cluster slabs table */
static unsigned int maxslabgrp; /* max # of entries in slabs table */
static unsigned int slabgrp;    /* # of entries in slabs table */

/* Globals */
unsigned char *mbutl;           /* first mapped cluster address */
static unsigned char *embutl;          /* ending virtual address of mclusters */

static boolean_t mclverify;     /* debug: pattern-checking */
static boolean_t mcltrace;      /* debug: stack tracing */
static boolean_t mclfindleak;   /* debug: leak detection */
static boolean_t mclexpleak;    /* debug: expose leak info to user space */

static struct timeval mb_start; /* beginning of time */

/* mbuf leak detection variables */
static struct mleak_table mleak_table;
static mleak_stat_t *mleak_stat;

#define MLEAK_STAT_SIZE(n) \
	__builtin_offsetof(mleak_stat_t, ml_trace[n])

struct mallocation {
	mcache_obj_t *element;  /* the alloc'ed element, NULL if unused */
	u_int32_t trace_index;  /* mtrace index for corresponding backtrace */
	u_int32_t count;        /* How many objects were requested */
	u_int64_t hitcount;     /* for determining hash effectiveness */
};

struct mtrace {
	u_int64_t       collisions;
	u_int64_t       hitcount;
	u_int64_t       allocs;
	u_int64_t       depth;
	uintptr_t       addr[MLEAK_STACK_DEPTH];
};

/* Size must be a power of two for the zhash to be able to just mask off bits */
#define MLEAK_ALLOCATION_MAP_NUM        512
#define MLEAK_TRACE_MAP_NUM             256

/*
 * Sample factor for how often to record a trace.  This is overwritable
 * by the boot-arg mleak_sample_factor.
 */
#define MLEAK_SAMPLE_FACTOR             500

/*
 * Number of top leakers recorded.
 */
#define MLEAK_NUM_TRACES                5

#define MB_LEAK_SPACING_64 "                    "
#define MB_LEAK_SPACING_32 "            "


#define MB_LEAK_HDR_32  "\n\
    trace [1]   trace [2]   trace [3]   trace [4]   trace [5]  \n\
    ----------  ----------  ----------  ----------  ---------- \n\
"

#define MB_LEAK_HDR_64  "\n\
    trace [1]           trace [2]           trace [3]       \
	trace [4]           trace [5]      \n\
    ------------------  ------------------  ------------------  \
    ------------------  ------------------ \n\
"

static uint32_t mleak_alloc_buckets = MLEAK_ALLOCATION_MAP_NUM;
static uint32_t mleak_trace_buckets = MLEAK_TRACE_MAP_NUM;

/* Hashmaps of allocations and their corresponding traces */
static struct mallocation *mleak_allocations;
static struct mtrace *mleak_traces;
static struct mtrace *mleak_top_trace[MLEAK_NUM_TRACES];

/* Lock to protect mleak tables from concurrent modification */
static LCK_GRP_DECLARE(mleak_lock_grp, "mleak_lock");
static LCK_MTX_DECLARE(mleak_lock_data, &mleak_lock_grp);
static lck_mtx_t *const mleak_lock = &mleak_lock_data;

/* *Failed* large allocations. */
struct mtracelarge {
	uint64_t        size;
	uint64_t        depth;
	uintptr_t       addr[MLEAK_STACK_DEPTH];
};

#define MTRACELARGE_NUM_TRACES          5
static struct mtracelarge mtracelarge_table[MTRACELARGE_NUM_TRACES];

static void mtracelarge_register(size_t size);

/* The minimum number of objects that are allocated, to start. */
#define MINCL           32
#define MINBIGCL        (MINCL >> 1)

/* Low watermarks (only map in pages once free counts go below) */
#define MBIGCL_LOWAT    MINBIGCL

#define m_cache(c)      mbuf_table[c].mtbl_cache
#define m_slablist(c)   mbuf_table[c].mtbl_slablist
#define m_cobjlist(c)   mbuf_table[c].mtbl_cobjlist
#define m_wantpurge(c)  mbuf_table[c].mtbl_wantpurge
#define m_active(c)     mbuf_table[c].mtbl_stats->mbcl_active
#define m_slab_cnt(c)   mbuf_table[c].mtbl_stats->mbcl_slab_cnt
#define m_alloc_cnt(c)  mbuf_table[c].mtbl_stats->mbcl_alloc_cnt
#define m_free_cnt(c)   mbuf_table[c].mtbl_stats->mbcl_free_cnt
#define m_notified(c)   mbuf_table[c].mtbl_stats->mbcl_notified
#define m_purge_cnt(c)  mbuf_table[c].mtbl_stats->mbcl_purge_cnt
#define m_fail_cnt(c)   mbuf_table[c].mtbl_stats->mbcl_fail_cnt
#define m_release_cnt(c) mbuf_table[c].mtbl_stats->mbcl_release_cnt
#define m_region_expand(c)      mbuf_table[c].mtbl_expand

mbuf_table_t mbuf_table[] = {
	/*
	 * The caches for mbufs, regular clusters and big clusters.
	 * The average total values were based on data gathered by actual
	 * usage patterns on iOS.
	 */
	{ MC_MBUF, NULL, TAILQ_HEAD_INITIALIZER(m_slablist(MC_MBUF)),
	  NULL, NULL, 0, 0, 0, 0, 3000, 0 },
	{ MC_CL, NULL, TAILQ_HEAD_INITIALIZER(m_slablist(MC_CL)),
	  NULL, NULL, 0, 0, 0, 0, 2000, 0 },
	{ MC_BIGCL, NULL, TAILQ_HEAD_INITIALIZER(m_slablist(MC_BIGCL)),
	  NULL, NULL, 0, 0, 0, 0, 1000, 0 },
	{ MC_16KCL, NULL, TAILQ_HEAD_INITIALIZER(m_slablist(MC_16KCL)),
	  NULL, NULL, 0, 0, 0, 0, 200, 0 },
	/*
	 * The following are special caches; they serve as intermediate
	 * caches backed by the above rudimentary caches.  Each object
	 * in the cache is an mbuf with a cluster attached to it.  Unlike
	 * the above caches, these intermediate caches do not directly
	 * deal with the slab structures; instead, the constructed
	 * cached elements are simply stored in the freelists.
	 */
	{ MC_MBUF_CL, NULL, { NULL, NULL }, NULL, NULL, 0, 0, 0, 0, 2000, 0 },
	{ MC_MBUF_BIGCL, NULL, { NULL, NULL }, NULL, NULL, 0, 0, 0, 0, 1000, 0 },
	{ MC_MBUF_16KCL, NULL, { NULL, NULL }, NULL, NULL, 0, 0, 0, 0, 200, 0 },
};

#if SKYWALK
#define MC_THRESHOLD_SCALE_DOWN_FACTOR  2
static unsigned int mc_threshold_scale_down_factor =
    MC_THRESHOLD_SCALE_DOWN_FACTOR;
#endif /* SKYWALK */

static uint32_t
m_avgtotal(mbuf_class_t c)
{
#if SKYWALK
	return if_is_fsw_transport_netagent_enabled() ?
	       (mbuf_table[c].mtbl_avgtotal / mc_threshold_scale_down_factor) :
	       mbuf_table[c].mtbl_avgtotal;
#else /* !SKYWALK */
	return mbuf_table[c].mtbl_avgtotal;
#endif /* SKYWALK */
}

static void *mb_waitchan = &mbuf_table; /* wait channel for all caches */
static int mb_waiters;                  /* number of waiters */

static struct timeval mb_wdtstart;      /* watchdog start timestamp */
static char *mbuf_dump_buf;

#define MBUF_DUMP_BUF_SIZE      4096

/*
 * mbuf watchdog is enabled by default.  It is also toggeable via the
 * kern.ipc.mb_watchdog sysctl.
 * Garbage collection is enabled by default on embedded platforms.
 * mb_drain_maxint controls the amount of time to wait (in seconds) before
 * consecutive calls to mbuf_drain().
 */
static unsigned int mb_watchdog = 1;
#if !XNU_TARGET_OS_OSX
static unsigned int mb_drain_maxint = 60;
#else /* XNU_TARGET_OS_OSX */
static unsigned int mb_drain_maxint = 0;
#endif /* XNU_TARGET_OS_OSX */

/* The following are used to serialize m_clalloc() */
static boolean_t mb_clalloc_busy;
static void *mb_clalloc_waitchan = &mb_clalloc_busy;
static int mb_clalloc_waiters;

static char *mbuf_dump(void);
static void mbuf_worker_thread_init(void);
static mcache_obj_t *slab_alloc(mbuf_class_t, int);
static void slab_free(mbuf_class_t, mcache_obj_t *);
static unsigned int mbuf_slab_alloc(void *, mcache_obj_t ***,
    unsigned int, int);
static void mbuf_slab_free(void *, mcache_obj_t *, int);
static void mbuf_slab_audit(void *, mcache_obj_t *, boolean_t);
static void mbuf_slab_notify(void *, u_int32_t);
static unsigned int cslab_alloc(mbuf_class_t, mcache_obj_t ***,
    unsigned int);
static unsigned int cslab_free(mbuf_class_t, mcache_obj_t *, int);
static unsigned int mbuf_cslab_alloc(void *, mcache_obj_t ***,
    unsigned int, int);
static void mbuf_cslab_free(void *, mcache_obj_t *, int);
static void mbuf_cslab_audit(void *, mcache_obj_t *, boolean_t);
static int freelist_populate(mbuf_class_t, unsigned int, int);
static void freelist_init(mbuf_class_t);
static boolean_t mbuf_cached_above(mbuf_class_t, int);
static boolean_t mbuf_steal(mbuf_class_t, unsigned int);
static void m_reclaim(mbuf_class_t, unsigned int, boolean_t);
static int m_howmany(int, size_t);
static void mbuf_worker_thread(void);
static void mbuf_watchdog(void);
static boolean_t mbuf_sleep(mbuf_class_t, unsigned int, int);

static void mcl_audit_init(void *, mcache_audit_t **, mcache_obj_t **,
    size_t, unsigned int);
static void mcl_audit_free(void *, unsigned int);
static mcache_audit_t *mcl_audit_buf2mca(mbuf_class_t, mcache_obj_t *);
static void mcl_audit_mbuf(mcache_audit_t *, void *, boolean_t, boolean_t);
static void mcl_audit_cluster(mcache_audit_t *, void *, size_t, boolean_t,
    boolean_t);
static void mcl_audit_restore_mbuf(struct mbuf *, mcache_audit_t *, boolean_t);
static void mcl_audit_save_mbuf(struct mbuf *, mcache_audit_t *);
static void mcl_audit_scratch(mcache_audit_t *);
static void mcl_audit_mcheck_panic(struct mbuf *);
static void mcl_audit_verify_nextptr(void *, mcache_audit_t *);

static void mleak_activate(void);
static void mleak_logger(u_int32_t, mcache_obj_t *, boolean_t);
static boolean_t mleak_log(uintptr_t *, mcache_obj_t *, uint32_t, int);
static void mleak_free(mcache_obj_t *);
static void mleak_sort_traces(void);
static void mleak_update_stats(void);

static mcl_slab_t *slab_get(void *);
static void slab_init(mcl_slab_t *, mbuf_class_t, u_int32_t,
    void *, void *, unsigned int, int, int);
static void slab_insert(mcl_slab_t *, mbuf_class_t);
static void slab_remove(mcl_slab_t *, mbuf_class_t);
static boolean_t slab_inrange(mcl_slab_t *, void *);
static void slab_nextptr_panic(mcl_slab_t *, void *);
static void slab_detach(mcl_slab_t *);
static boolean_t slab_is_detached(mcl_slab_t *);

#if (DEBUG || DEVELOPMENT)
#define mbwdog_logger(fmt, ...)  _mbwdog_logger(__func__, __LINE__, fmt, ## __VA_ARGS__)
static void _mbwdog_logger(const char *func, const int line, const char *fmt, ...);
static char *mbwdog_logging;
const unsigned mbwdog_logging_size = 4096;
static size_t mbwdog_logging_used;
#else
#define mbwdog_logger(fmt, ...)  do { } while (0)
#endif /* DEBUG || DEVELOPMENT */
static void mbuf_drain_locked(boolean_t);

void
mbuf_mcheck(struct mbuf *m)
{
	if (__improbable(m->m_type != MT_FREE && !MBUF_IS_PAIRED(m))) {
		if (mclaudit == NULL) {
			panic("MCHECK: m_type=%d m=%p",
			    (u_int16_t)(m)->m_type, m);
		} else {
			mcl_audit_mcheck_panic(m);
		}
	}
}

#define MBUF_IN_MAP(addr)                                               \
	((unsigned char *)(addr) >= mbutl &&                            \
	(unsigned char *)(addr) < embutl)

#define MRANGE(addr) {                                                  \
	if (!MBUF_IN_MAP(addr))                                         \
	        panic("MRANGE: address out of range 0x%p", addr);       \
}

/*
 * Macros to obtain page index given a base cluster address
 */
#define MTOPG(x)        (((unsigned char *)x - mbutl) >> PAGE_SHIFT)
#define PGTOM(x)        (mbutl + (x << PAGE_SHIFT))

/*
 * Macro to find the mbuf index relative to a base.
 */
#define MBPAGEIDX(c, m) \
	(((unsigned char *)(m) - (unsigned char *)(c)) >> _MSIZESHIFT)

/*
 * Same thing for 2KB cluster index.
 */
#define CLPAGEIDX(c, m) \
	(((unsigned char *)(m) - (unsigned char *)(c)) >> MCLSHIFT)

/*
 * Macro to find 4KB cluster index relative to a base
 */
#define BCLPAGEIDX(c, m) \
	(((unsigned char *)(m) - (unsigned char *)(c)) >> MBIGCLSHIFT)

/*
 * Macro to convert BSD malloc sleep flag to mcache's
 */
#define MSLEEPF(f)      ((!((f) & M_DONTWAIT)) ? MCR_SLEEP : MCR_NOSLEEP)

static int
mleak_top_trace_sysctl SYSCTL_HANDLER_ARGS
{
#pragma unused(oidp, arg1, arg2)
	int i;

	/* Ensure leak tracing turned on */
	if (!mclfindleak || !mclexpleak) {
		return ENXIO;
	}

	lck_mtx_lock(mleak_lock);
	mleak_update_stats();
	i = SYSCTL_OUT(req, mleak_stat, MLEAK_STAT_SIZE(MLEAK_NUM_TRACES));
	lck_mtx_unlock(mleak_lock);

	return i;
}

static int
mleak_table_sysctl SYSCTL_HANDLER_ARGS
{
#pragma unused(oidp, arg1, arg2)
	int i = 0;

	/* Ensure leak tracing turned on */
	if (!mclfindleak || !mclexpleak) {
		return ENXIO;
	}

	lck_mtx_lock(mleak_lock);
	i = SYSCTL_OUT(req, &mleak_table, sizeof(mleak_table));
	lck_mtx_unlock(mleak_lock);

	return i;
}

void
mbuf_stat_sync(void)
{
	mb_class_stat_t *sp;
	mcache_cpu_t *ccp;
	mcache_t *cp;
	int k, m, bktsize;


	LCK_MTX_ASSERT(mbuf_mlock, LCK_MTX_ASSERT_OWNED);

	for (k = 0; k < MC_MAX; k++) {
		cp = m_cache(k);
		ccp = &cp->mc_cpu[0];
		bktsize = ccp->cc_bktsize;
		sp = mbuf_table[k].mtbl_stats;

		if (cp->mc_flags & MCF_NOCPUCACHE) {
			sp->mbcl_mc_state = MCS_DISABLED;
		} else if (cp->mc_purge_cnt > 0) {
			sp->mbcl_mc_state = MCS_PURGING;
		} else if (bktsize == 0) {
			sp->mbcl_mc_state = MCS_OFFLINE;
		} else {
			sp->mbcl_mc_state = MCS_ONLINE;
		}

		sp->mbcl_mc_cached = 0;
		for (m = 0; m < ncpu; m++) {
			ccp = &cp->mc_cpu[m];
			if (ccp->cc_objs > 0) {
				sp->mbcl_mc_cached += ccp->cc_objs;
			}
			if (ccp->cc_pobjs > 0) {
				sp->mbcl_mc_cached += ccp->cc_pobjs;
			}
		}
		sp->mbcl_mc_cached += (cp->mc_full.bl_total * bktsize);
		sp->mbcl_active = sp->mbcl_total - sp->mbcl_mc_cached -
		    sp->mbcl_infree;

		sp->mbcl_mc_waiter_cnt = cp->mc_waiter_cnt;
		sp->mbcl_mc_wretry_cnt = cp->mc_wretry_cnt;
		sp->mbcl_mc_nwretry_cnt = cp->mc_nwretry_cnt;

		/* Calculate total count specific to each class */
		sp->mbcl_ctotal = sp->mbcl_total;
		switch (m_class(k)) {
		case MC_MBUF:
			/* Deduct mbufs used in composite caches */
			sp->mbcl_ctotal -= (m_total(MC_MBUF_CL) +
			    m_total(MC_MBUF_BIGCL) - m_total(MC_MBUF_16KCL));
			break;

		case MC_CL:
			/* Deduct clusters used in composite cache */
			sp->mbcl_ctotal -= m_total(MC_MBUF_CL);
			break;

		case MC_BIGCL:
			/* Deduct clusters used in composite cache */
			sp->mbcl_ctotal -= m_total(MC_MBUF_BIGCL);
			break;

		case MC_16KCL:
			/* Deduct clusters used in composite cache */
			sp->mbcl_ctotal -= m_total(MC_MBUF_16KCL);
			break;

		default:
			break;
		}
	}
}

bool
mbuf_class_under_pressure(struct mbuf *m)
{
	int mclass = mbuf_get_class(m);

	if (m_total(mclass) - m_infree(mclass) >= (m_maxlimit(mclass) * mb_memory_pressure_percentage) / 100) {
		/*
		 * The above computation does not include the per-CPU cached objects.
		 * As a fast-path check this is good-enough. But now we do
		 * the "slower" count of the cached objects to know exactly the
		 * number of active mbufs in use.
		 *
		 * We do not take the mbuf_lock here to avoid lock-contention. Numbers
		 * might be slightly off but we don't try to be 100% accurate.
		 * At worst, we drop a packet that we shouldn't have dropped or
		 * we might go slightly above our memory-pressure threshold.
		 */
		mcache_t *cp = m_cache(mclass);
		mcache_cpu_t *ccp = &cp->mc_cpu[0];

		int bktsize = os_access_once(ccp->cc_bktsize);
		uint32_t bl_total = os_access_once(cp->mc_full.bl_total);
		uint32_t cached = 0;
		int i;

		for (i = 0; i < ncpu; i++) {
			ccp = &cp->mc_cpu[i];

			int cc_objs = os_access_once(ccp->cc_objs);
			if (cc_objs > 0) {
				cached += cc_objs;
			}

			int cc_pobjs = os_access_once(ccp->cc_pobjs);
			if (cc_pobjs > 0) {
				cached += cc_pobjs;
			}
		}
		cached += (bl_total * bktsize);
		if (m_total(mclass) - m_infree(mclass) - cached >= (m_maxlimit(mclass) * mb_memory_pressure_percentage) / 100) {
			os_log(OS_LOG_DEFAULT,
			    "%s memory-pressure on mbuf due to class %u, total %u free %u cached %u max %u",
			    __func__, mclass, m_total(mclass), m_infree(mclass), cached, m_maxlimit(mclass));
			return true;
		}
	}

	return false;
}

__private_extern__ void
mbinit(void)
{
	unsigned int m;
	unsigned int initmcl = 0;
	thread_t thread = THREAD_NULL;

	microuptime(&mb_start);

	/*
	 * These MBUF_ values must be equal to their private counterparts.
	 */
	static_assert(MBUF_EXT == M_EXT);
	static_assert(MBUF_PKTHDR == M_PKTHDR);
	static_assert(MBUF_EOR == M_EOR);
	static_assert(MBUF_LOOP == M_LOOP);
	static_assert(MBUF_BCAST == M_BCAST);
	static_assert(MBUF_MCAST == M_MCAST);
	static_assert(MBUF_FRAG == M_FRAG);
	static_assert(MBUF_FIRSTFRAG == M_FIRSTFRAG);
	static_assert(MBUF_LASTFRAG == M_LASTFRAG);
	static_assert(MBUF_PROMISC == M_PROMISC);
	static_assert(MBUF_HASFCS == M_HASFCS);

	static_assert(MBUF_TYPE_FREE == MT_FREE);
	static_assert(MBUF_TYPE_DATA == MT_DATA);
	static_assert(MBUF_TYPE_HEADER == MT_HEADER);
	static_assert(MBUF_TYPE_SOCKET == MT_SOCKET);
	static_assert(MBUF_TYPE_PCB == MT_PCB);
	static_assert(MBUF_TYPE_RTABLE == MT_RTABLE);
	static_assert(MBUF_TYPE_HTABLE == MT_HTABLE);
	static_assert(MBUF_TYPE_ATABLE == MT_ATABLE);
	static_assert(MBUF_TYPE_SONAME == MT_SONAME);
	static_assert(MBUF_TYPE_SOOPTS == MT_SOOPTS);
	static_assert(MBUF_TYPE_FTABLE == MT_FTABLE);
	static_assert(MBUF_TYPE_RIGHTS == MT_RIGHTS);
	static_assert(MBUF_TYPE_IFADDR == MT_IFADDR);
	static_assert(MBUF_TYPE_CONTROL == MT_CONTROL);
	static_assert(MBUF_TYPE_OOBDATA == MT_OOBDATA);

	static_assert(MBUF_TSO_IPV4 == CSUM_TSO_IPV4);
	static_assert(MBUF_TSO_IPV6 == CSUM_TSO_IPV6);
	static_assert(MBUF_CSUM_REQ_SUM16 == CSUM_PARTIAL);
	static_assert(MBUF_CSUM_TCP_SUM16 == MBUF_CSUM_REQ_SUM16);
	static_assert(MBUF_CSUM_REQ_ZERO_INVERT == CSUM_ZERO_INVERT);
	static_assert(MBUF_CSUM_REQ_IP == CSUM_IP);
	static_assert(MBUF_CSUM_REQ_TCP == CSUM_TCP);
	static_assert(MBUF_CSUM_REQ_UDP == CSUM_UDP);
	static_assert(MBUF_CSUM_REQ_TCPIPV6 == CSUM_TCPIPV6);
	static_assert(MBUF_CSUM_REQ_UDPIPV6 == CSUM_UDPIPV6);
	static_assert(MBUF_CSUM_DID_IP == CSUM_IP_CHECKED);
	static_assert(MBUF_CSUM_IP_GOOD == CSUM_IP_VALID);
	static_assert(MBUF_CSUM_DID_DATA == CSUM_DATA_VALID);
	static_assert(MBUF_CSUM_PSEUDO_HDR == CSUM_PSEUDO_HDR);

	static_assert(MBUF_WAITOK == M_WAIT);
	static_assert(MBUF_DONTWAIT == M_DONTWAIT);
	static_assert(MBUF_COPYALL == M_COPYALL);

	static_assert(MBUF_SC2TC(MBUF_SC_BK_SYS) == MBUF_TC_BK);
	static_assert(MBUF_SC2TC(MBUF_SC_BK) == MBUF_TC_BK);
	static_assert(MBUF_SC2TC(MBUF_SC_BE) == MBUF_TC_BE);
	static_assert(MBUF_SC2TC(MBUF_SC_RD) == MBUF_TC_BE);
	static_assert(MBUF_SC2TC(MBUF_SC_OAM) == MBUF_TC_BE);
	static_assert(MBUF_SC2TC(MBUF_SC_AV) == MBUF_TC_VI);
	static_assert(MBUF_SC2TC(MBUF_SC_RV) == MBUF_TC_VI);
	static_assert(MBUF_SC2TC(MBUF_SC_VI) == MBUF_TC_VI);
	static_assert(MBUF_SC2TC(MBUF_SC_SIG) == MBUF_TC_VI);
	static_assert(MBUF_SC2TC(MBUF_SC_VO) == MBUF_TC_VO);
	static_assert(MBUF_SC2TC(MBUF_SC_CTL) == MBUF_TC_VO);

	static_assert(MBUF_TC2SCVAL(MBUF_TC_BK) == SCVAL_BK);
	static_assert(MBUF_TC2SCVAL(MBUF_TC_BE) == SCVAL_BE);
	static_assert(MBUF_TC2SCVAL(MBUF_TC_VI) == SCVAL_VI);
	static_assert(MBUF_TC2SCVAL(MBUF_TC_VO) == SCVAL_VO);

	/* Module specific scratch space (32-bit alignment requirement) */
	static_assert(!(offsetof(struct mbuf, m_pkthdr.pkt_mpriv) % sizeof(uint32_t)));

	/* Make sure we don't save more than we should */
	static_assert(MCA_SAVED_MBUF_SIZE <= sizeof(struct mbuf));

	if (nmbclusters == 0) {
		nmbclusters = NMBCLUSTERS;
	}

	/* This should be a sane (at least even) value by now */
	VERIFY(nmbclusters != 0 && !(nmbclusters & 0x1));

	/* Setup the mbuf table */
	mbuf_table_init();

	static_assert(sizeof(struct mbuf) == _MSIZE);

	/*
	 * Allocate cluster slabs table:
	 *
	 *	maxslabgrp = (N * 2048) / (1024 * 1024)
	 *
	 * Where N is nmbclusters rounded up to the nearest 512.  This yields
	 * mcl_slab_g_t units, each one representing a MB of memory.
	 */
	maxslabgrp =
	    (P2ROUNDUP(nmbclusters, (MBSIZE >> MCLSHIFT)) << MCLSHIFT) >> MBSHIFT;
	slabstbl = zalloc_permanent(maxslabgrp * sizeof(mcl_slabg_t *),
	    ZALIGN(mcl_slabg_t));

	/*
	 * Allocate audit structures, if needed:
	 *
	 *	maxclaudit = (maxslabgrp * 1024 * 1024) / PAGE_SIZE
	 *
	 * This yields mcl_audit_t units, each one representing a page.
	 */
	PE_parse_boot_argn("mbuf_debug", &mbuf_debug, sizeof(mbuf_debug));
	mbuf_debug |= mcache_getflags();
	if (mbuf_debug & MCF_DEBUG) {
		int l;
		mcl_audit_t *mclad;
		maxclaudit = ((maxslabgrp << MBSHIFT) >> PAGE_SHIFT);
		mclaudit = zalloc_permanent(maxclaudit * sizeof(*mclaudit),
		    ZALIGN(mcl_audit_t));
		for (l = 0, mclad = mclaudit; l < maxclaudit; l++) {
			mclad[l].cl_audit = zalloc_permanent(NMBPG * sizeof(mcache_audit_t *),
			    ZALIGN_PTR);
		}

		mcl_audit_con_cache = mcache_create("mcl_audit_contents",
		    AUDIT_CONTENTS_SIZE, sizeof(u_int64_t), 0, MCR_SLEEP);
		VERIFY(mcl_audit_con_cache != NULL);
	}
	mclverify = (mbuf_debug & MCF_VERIFY);
	mcltrace = (mbuf_debug & MCF_TRACE);
	mclfindleak = !(mbuf_debug & MCF_NOLEAKLOG);
	mclexpleak = mclfindleak && (mbuf_debug & MCF_EXPLEAKLOG);

	/* Enable mbuf leak logging, with a lock to protect the tables */

	mleak_activate();

	/*
	 * Allocate structure for per-CPU statistics that's aligned
	 * on the CPU cache boundary; this code assumes that we never
	 * uninitialize this framework, since the original address
	 * before alignment is not saved.
	 */
	ncpu = ml_wait_max_cpus();

	/* Calculate the number of pages assigned to the cluster pool */
	mcl_pages = (nmbclusters << MCLSHIFT) / PAGE_SIZE;
	mcl_paddr = zalloc_permanent(mcl_pages * sizeof(ppnum_t),
	    ZALIGN(ppnum_t));

	/* Register with the I/O Bus mapper */
	mcl_paddr_base = IOMapperIOVMAlloc(mcl_pages);

	embutl = (mbutl + (nmbclusters * MCLBYTES));
	VERIFY(((embutl - mbutl) % MBIGCLBYTES) == 0);

	/* Prime up the freelist */
	PE_parse_boot_argn("initmcl", &initmcl, sizeof(initmcl));
	if (initmcl != 0) {
		initmcl >>= NCLPBGSHIFT;        /* become a 4K unit */
		if (initmcl > m_maxlimit(MC_BIGCL)) {
			initmcl = m_maxlimit(MC_BIGCL);
		}
	}
	if (initmcl < m_minlimit(MC_BIGCL)) {
		initmcl = m_minlimit(MC_BIGCL);
	}

	lck_mtx_lock(mbuf_mlock);

	/*
	 * For classes with non-zero minimum limits, populate their freelists
	 * so that m_total(class) is at least m_minlimit(class).
	 */
	VERIFY(m_total(MC_BIGCL) == 0 && m_minlimit(MC_BIGCL) != 0);
	freelist_populate(m_class(MC_BIGCL), initmcl, M_WAIT);
	VERIFY(m_total(MC_BIGCL) >= m_minlimit(MC_BIGCL));
	freelist_init(m_class(MC_CL));

	for (m = 0; m < MC_MAX; m++) {
		/* Make sure we didn't miss any */
		VERIFY(m_minlimit(m_class(m)) == 0 ||
		    m_total(m_class(m)) >= m_minlimit(m_class(m)));
	}

	lck_mtx_unlock(mbuf_mlock);

	(void) kernel_thread_start((thread_continue_t)mbuf_worker_thread_init,
	    NULL, &thread);
	thread_deallocate(thread);

	ref_cache = mcache_create("mext_ref", sizeof(struct ext_ref),
	    0, 0, MCR_SLEEP);

	/* Create the cache for each class */
	for (m = 0; m < MC_MAX; m++) {
		void *allocfunc, *freefunc, *auditfunc, *logfunc;
		u_int32_t flags;

		flags = mbuf_debug;
		if (m_class(m) == MC_MBUF_CL || m_class(m) == MC_MBUF_BIGCL ||
		    m_class(m) == MC_MBUF_16KCL) {
			allocfunc = mbuf_cslab_alloc;
			freefunc = mbuf_cslab_free;
			auditfunc = mbuf_cslab_audit;
			logfunc = mleak_logger;
		} else {
			allocfunc = mbuf_slab_alloc;
			freefunc = mbuf_slab_free;
			auditfunc = mbuf_slab_audit;
			logfunc = mleak_logger;
		}

		if (!mclfindleak) {
			flags |= MCF_NOLEAKLOG;
		}

		m_cache(m) = mcache_create_ext(m_cname(m), m_maxsize(m),
		    allocfunc, freefunc, auditfunc, logfunc, mbuf_slab_notify,
		    (void *)(uintptr_t)m, flags, MCR_SLEEP);
	}

	/*
	 * Set the max limit on sb_max to be 1/16 th of the size of
	 * memory allocated for mbuf clusters.
	 */
	high_sb_max = (nmbclusters << (MCLSHIFT - 4));
	if (high_sb_max < sb_max) {
		/* sb_max is too large for this configuration, scale it down */
		if (high_sb_max > (1 << MBSHIFT)) {
			/* We have atleast 16 M of mbuf pool */
			sb_max = high_sb_max;
		} else if ((nmbclusters << MCLSHIFT) > (1 << MBSHIFT)) {
			/*
			 * If we have more than 1M of mbufpool, cap the size of
			 * max sock buf at 1M
			 */
			sb_max = high_sb_max = (1 << MBSHIFT);
		} else {
			sb_max = high_sb_max;
		}
	}

	/* allocate space for mbuf_dump_buf */
	mbuf_dump_buf = zalloc_permanent(MBUF_DUMP_BUF_SIZE, ZALIGN_NONE);

	if (mbuf_debug & MCF_DEBUG) {
		printf("%s: MLEN %d, MHLEN %d\n", __func__,
		    (int)_MLEN, (int)_MHLEN);
	}
	printf("%s: done [%d MB total pool size, (%d/%d) split]\n", __func__,
	    (nmbclusters << MCLSHIFT) >> MBSHIFT,
	    (nclusters << MCLSHIFT) >> MBSHIFT,
	    (njcl << MCLSHIFT) >> MBSHIFT);
}

/*
 * Obtain a slab of object(s) from the class's freelist.
 */
static mcache_obj_t *
slab_alloc(mbuf_class_t class, int wait)
{
	mcl_slab_t *sp;
	mcache_obj_t *buf;

	LCK_MTX_ASSERT(mbuf_mlock, LCK_MTX_ASSERT_OWNED);

	/* This should always be NULL for us */
	VERIFY(m_cobjlist(class) == NULL);

	/*
	 * Treat composite objects as having longer lifespan by using
	 * a slab from the reverse direction, in hoping that this could
	 * reduce the probability of fragmentation for slabs that hold
	 * more than one buffer chunks (e.g. mbuf slabs).  For other
	 * slabs, this probably doesn't make much of a difference.
	 */
	if ((class == MC_MBUF || class == MC_CL || class == MC_BIGCL)
	    && (wait & MCR_COMP)) {
		sp = (mcl_slab_t *)TAILQ_LAST(&m_slablist(class), mcl_slhead);
	} else {
		sp = (mcl_slab_t *)TAILQ_FIRST(&m_slablist(class));
	}

	if (sp == NULL) {
		VERIFY(m_infree(class) == 0 && m_slab_cnt(class) == 0);
		/* The slab list for this class is empty */
		return NULL;
	}

	VERIFY(m_infree(class) > 0);
	VERIFY(!slab_is_detached(sp));
	VERIFY(sp->sl_class == class &&
	    (sp->sl_flags & (SLF_MAPPED | SLF_PARTIAL)) == SLF_MAPPED);
	buf = sp->sl_head;
	VERIFY(slab_inrange(sp, buf) && sp == slab_get(buf));
	sp->sl_head = buf->obj_next;
	/* Increment slab reference */
	sp->sl_refcnt++;

	VERIFY(sp->sl_head != NULL || sp->sl_refcnt == sp->sl_chunks);

	if (sp->sl_head != NULL && !slab_inrange(sp, sp->sl_head)) {
		slab_nextptr_panic(sp, sp->sl_head);
		/* In case sl_head is in the map but not in the slab */
		VERIFY(slab_inrange(sp, sp->sl_head));
		/* NOTREACHED */
	}

	if (mclaudit != NULL) {
		mcache_audit_t *mca = mcl_audit_buf2mca(class, buf);
		mca->mca_uflags = 0;
		/* Save contents on mbuf objects only */
		if (class == MC_MBUF) {
			mca->mca_uflags |= MB_SCVALID;
		}
	}

	if (class == MC_CL) {
		mbstat.m_clfree = (--m_infree(MC_CL)) + m_infree(MC_MBUF_CL);
		/*
		 * A 2K cluster slab can have at most NCLPG references.
		 */
		VERIFY(sp->sl_refcnt >= 1 && sp->sl_refcnt <= NCLPG &&
		    sp->sl_chunks == NCLPG && sp->sl_len == PAGE_SIZE);
		VERIFY(sp->sl_refcnt < NCLPG || sp->sl_head == NULL);
	} else if (class == MC_BIGCL) {
		mbstat.m_bigclfree = (--m_infree(MC_BIGCL)) +
		    m_infree(MC_MBUF_BIGCL);
		/*
		 * A 4K cluster slab can have NBCLPG references.
		 */
		VERIFY(sp->sl_refcnt >= 1 && sp->sl_chunks == NBCLPG &&
		    sp->sl_len == PAGE_SIZE &&
		    (sp->sl_refcnt < NBCLPG || sp->sl_head == NULL));
	} else if (class == MC_16KCL) {
		mcl_slab_t *nsp;
		int k;

		--m_infree(MC_16KCL);
		VERIFY(sp->sl_refcnt == 1 && sp->sl_chunks == 1 &&
		    sp->sl_len == m_maxsize(class) && sp->sl_head == NULL);
		/*
		 * Increment 2nd-Nth slab reference, where N is NSLABSP16KB.
		 * A 16KB big cluster takes NSLABSP16KB slabs, each having at
		 * most 1 reference.
		 */
		for (nsp = sp, k = 1; k < NSLABSP16KB; k++) {
			nsp = nsp->sl_next;
			/* Next slab must already be present */
			VERIFY(nsp != NULL);
			nsp->sl_refcnt++;
			VERIFY(!slab_is_detached(nsp));
			VERIFY(nsp->sl_class == MC_16KCL &&
			    nsp->sl_flags == (SLF_MAPPED | SLF_PARTIAL) &&
			    nsp->sl_refcnt == 1 && nsp->sl_chunks == 0 &&
			    nsp->sl_len == 0 && nsp->sl_base == sp->sl_base &&
			    nsp->sl_head == NULL);
		}
	} else {
		VERIFY(class == MC_MBUF);
		--m_infree(MC_MBUF);
		/*
		 * If auditing is turned on, this check is
		 * deferred until later in mbuf_slab_audit().
		 */
		if (mclaudit == NULL) {
			mbuf_mcheck((struct mbuf *)buf);
		}
		/*
		 * Since we have incremented the reference count above,
		 * an mbuf slab (formerly a 4KB cluster slab that was cut
		 * up into mbufs) must have a reference count between 1
		 * and NMBPG at this point.
		 */
		VERIFY(sp->sl_refcnt >= 1 && sp->sl_refcnt <= NMBPG &&
		    sp->sl_chunks == NMBPG &&
		    sp->sl_len == PAGE_SIZE);
		VERIFY(sp->sl_refcnt < NMBPG || sp->sl_head == NULL);
	}

	/* If empty, remove this slab from the class's freelist */
	if (sp->sl_head == NULL) {
		VERIFY(class != MC_MBUF || sp->sl_refcnt == NMBPG);
		VERIFY(class != MC_CL || sp->sl_refcnt == NCLPG);
		VERIFY(class != MC_BIGCL || sp->sl_refcnt == NBCLPG);
		slab_remove(sp, class);
	}

	return buf;
}

/*
 * Place a slab of object(s) back into a class's slab list.
 */
static void
slab_free(mbuf_class_t class, mcache_obj_t *buf)
{
	mcl_slab_t *sp;
	boolean_t reinit_supercl = false;
	mbuf_class_t super_class;

	LCK_MTX_ASSERT(mbuf_mlock, LCK_MTX_ASSERT_OWNED);

	VERIFY(buf->obj_next == NULL);

	/*
	 * Synchronizing with m_clalloc, as it reads m_total, while we here
	 * are modifying m_total.
	 */
	while (mb_clalloc_busy) {
		mb_clalloc_waiters++;
		(void) msleep(mb_clalloc_waitchan, mbuf_mlock,
		    (PZERO - 1), "m_clalloc", NULL);
		LCK_MTX_ASSERT(mbuf_mlock, LCK_MTX_ASSERT_OWNED);
	}

	/* We are busy now; tell everyone else to go away */
	mb_clalloc_busy = TRUE;

	sp = slab_get(buf);
	VERIFY(sp->sl_class == class && slab_inrange(sp, buf) &&
	    (sp->sl_flags & (SLF_MAPPED | SLF_PARTIAL)) == SLF_MAPPED);

	/* Decrement slab reference */
	sp->sl_refcnt--;

	if (class == MC_CL) {
		VERIFY(IS_P2ALIGNED(buf, MCLBYTES));
		/*
		 * A slab that has been splitted for 2KB clusters can have
		 * at most 1 outstanding reference at this point.
		 */
		VERIFY(sp->sl_refcnt >= 0 && sp->sl_refcnt <= (NCLPG - 1) &&
		    sp->sl_chunks == NCLPG && sp->sl_len == PAGE_SIZE);
		VERIFY(sp->sl_refcnt < (NCLPG - 1) ||
		    (slab_is_detached(sp) && sp->sl_head == NULL));
	} else if (class == MC_BIGCL) {
		VERIFY(IS_P2ALIGNED(buf, MBIGCLBYTES));

		/* A 4KB cluster slab can have NBCLPG references at most */
		VERIFY(sp->sl_refcnt >= 0 && sp->sl_chunks == NBCLPG);
		VERIFY(sp->sl_refcnt < (NBCLPG - 1) ||
		    (slab_is_detached(sp) && sp->sl_head == NULL));
	} else if (class == MC_16KCL) {
		mcl_slab_t *nsp;
		int k;
		/*
		 * A 16KB cluster takes NSLABSP16KB slabs, all must
		 * now have 0 reference.
		 */
		VERIFY(IS_P2ALIGNED(buf, PAGE_SIZE));
		VERIFY(sp->sl_refcnt == 0 && sp->sl_chunks == 1 &&
		    sp->sl_len == m_maxsize(class) && sp->sl_head == NULL);
		VERIFY(slab_is_detached(sp));
		for (nsp = sp, k = 1; k < NSLABSP16KB; k++) {
			nsp = nsp->sl_next;
			/* Next slab must already be present */
			VERIFY(nsp != NULL);
			nsp->sl_refcnt--;
			VERIFY(slab_is_detached(nsp));
			VERIFY(nsp->sl_class == MC_16KCL &&
			    (nsp->sl_flags & (SLF_MAPPED | SLF_PARTIAL)) &&
			    nsp->sl_refcnt == 0 && nsp->sl_chunks == 0 &&
			    nsp->sl_len == 0 && nsp->sl_base == sp->sl_base &&
			    nsp->sl_head == NULL);
		}
	} else {
		/*
		 * A slab that has been splitted for mbufs has at most
		 * NMBPG reference counts.  Since we have decremented
		 * one reference above, it must now be between 0 and
		 * NMBPG-1.
		 */
		VERIFY(class == MC_MBUF);
		VERIFY(sp->sl_refcnt >= 0 &&
		    sp->sl_refcnt <= (NMBPG - 1) &&
		    sp->sl_chunks == NMBPG &&
		    sp->sl_len == PAGE_SIZE);
		VERIFY(sp->sl_refcnt < (NMBPG - 1) ||
		    (slab_is_detached(sp) && sp->sl_head == NULL));
	}

	/*
	 * When auditing is enabled, ensure that the buffer still
	 * contains the free pattern.  Otherwise it got corrupted
	 * while at the CPU cache layer.
	 */
	if (mclaudit != NULL) {
		mcache_audit_t *mca = mcl_audit_buf2mca(class, buf);
		if (mclverify) {
			mcache_audit_free_verify(mca, buf, 0,
			    m_maxsize(class));
		}
		mca->mca_uflags &= ~MB_SCVALID;
	}

	if (class == MC_CL) {
		mbstat.m_clfree = (++m_infree(MC_CL)) + m_infree(MC_MBUF_CL);
		buf->obj_next = sp->sl_head;
	} else if (class == MC_BIGCL) {
		mbstat.m_bigclfree = (++m_infree(MC_BIGCL)) +
		    m_infree(MC_MBUF_BIGCL);
		buf->obj_next = sp->sl_head;
	} else if (class == MC_16KCL) {
		++m_infree(MC_16KCL);
	} else {
		++m_infree(MC_MBUF);
		buf->obj_next = sp->sl_head;
	}
	sp->sl_head = buf;

	/*
	 * If a slab has been split to either one which holds 2KB clusters,
	 * or one which holds mbufs, turn it back to one which holds a
	 * 4 or 16 KB cluster depending on the page size.
	 */
	if (m_maxsize(MC_BIGCL) == PAGE_SIZE) {
		super_class = MC_BIGCL;
	} else {
		VERIFY(PAGE_SIZE == m_maxsize(MC_16KCL));
		super_class = MC_16KCL;
	}
	if (class == MC_MBUF && sp->sl_refcnt == 0 &&
	    m_total(class) >= (m_minlimit(class) + NMBPG) &&
	    m_total(super_class) < m_maxlimit(super_class)) {
		int i = NMBPG;

		m_total(MC_MBUF) -= NMBPG;
		mbstat.m_mbufs = m_total(MC_MBUF);
		m_infree(MC_MBUF) -= NMBPG;
		mtype_stat_add(MT_FREE, -((unsigned)NMBPG));

		while (i--) {
			struct mbuf *m = sp->sl_head;
			VERIFY(m != NULL);
			sp->sl_head = m->m_next;
			m->m_next = NULL;
		}
		reinit_supercl = true;
	} else if (class == MC_CL && sp->sl_refcnt == 0 &&
	    m_total(class) >= (m_minlimit(class) + NCLPG) &&
	    m_total(super_class) < m_maxlimit(super_class)) {
		int i = NCLPG;

		m_total(MC_CL) -= NCLPG;
		mbstat.m_clusters = m_total(MC_CL);
		m_infree(MC_CL) -= NCLPG;

		while (i--) {
			union mcluster *c = sp->sl_head;
			VERIFY(c != NULL);
			sp->sl_head = c->mcl_next;
			c->mcl_next = NULL;
		}
		reinit_supercl = true;
	} else if (class == MC_BIGCL && super_class != MC_BIGCL &&
	    sp->sl_refcnt == 0 &&
	    m_total(class) >= (m_minlimit(class) + NBCLPG) &&
	    m_total(super_class) < m_maxlimit(super_class)) {
		int i = NBCLPG;

		VERIFY(super_class == MC_16KCL);
		m_total(MC_BIGCL) -= NBCLPG;
		mbstat.m_bigclusters = m_total(MC_BIGCL);
		m_infree(MC_BIGCL) -= NBCLPG;

		while (i--) {
			union mbigcluster *bc = sp->sl_head;
			VERIFY(bc != NULL);
			sp->sl_head = bc->mbc_next;
			bc->mbc_next = NULL;
		}
		reinit_supercl = true;
	}

	if (reinit_supercl) {
		VERIFY(sp->sl_head == NULL);
		VERIFY(m_total(class) >= m_minlimit(class));
		slab_remove(sp, class);

		/* Reinitialize it as a cluster for the super class */
		m_total(super_class)++;
		m_infree(super_class)++;
		VERIFY(sp->sl_flags == (SLF_MAPPED | SLF_DETACHED) &&
		    sp->sl_len == PAGE_SIZE && sp->sl_refcnt == 0);

		slab_init(sp, super_class, SLF_MAPPED, sp->sl_base,
		    sp->sl_base, PAGE_SIZE, 0, 1);
		if (mclverify) {
			mcache_set_pattern(MCACHE_FREE_PATTERN,
			    (caddr_t)sp->sl_base, sp->sl_len);
		}
		((mcache_obj_t *)(sp->sl_base))->obj_next = NULL;

		if (super_class == MC_BIGCL) {
			mbstat.m_bigclusters = m_total(MC_BIGCL);
			mbstat.m_bigclfree = m_infree(MC_BIGCL) +
			    m_infree(MC_MBUF_BIGCL);
		}

		VERIFY(slab_is_detached(sp));
		VERIFY(m_total(super_class) <= m_maxlimit(super_class));

		/* And finally switch class */
		class = super_class;
	}

	/* Reinsert the slab to the class's slab list */
	if (slab_is_detached(sp)) {
		slab_insert(sp, class);
	}

	/* We're done; let others enter */
	mb_clalloc_busy = FALSE;
	if (mb_clalloc_waiters > 0) {
		mb_clalloc_waiters = 0;
		wakeup(mb_clalloc_waitchan);
	}
}

/*
 * Common allocator for rudimentary objects called by the CPU cache layer
 * during an allocation request whenever there is no available element in the
 * bucket layer.  It returns one or more elements from the appropriate global
 * freelist.  If the freelist is empty, it will attempt to populate it and
 * retry the allocation.
 */
static unsigned int
mbuf_slab_alloc(void *arg, mcache_obj_t ***plist, unsigned int num, int wait)
{
	mbuf_class_t class = (mbuf_class_t)arg;
	unsigned int need = num;
	mcache_obj_t **list = *plist;

	ASSERT(MBUF_CLASS_VALID(class) && !MBUF_CLASS_COMPOSITE(class));
	ASSERT(need > 0);

	lck_mtx_lock(mbuf_mlock);

	for (;;) {
		if ((*list = slab_alloc(class, wait)) != NULL) {
			(*list)->obj_next = NULL;
			list = *plist = &(*list)->obj_next;

			if (--need == 0) {
				/*
				 * If the number of elements in freelist has
				 * dropped below low watermark, asynchronously
				 * populate the freelist now rather than doing
				 * it later when we run out of elements.
				 */
				if (!mbuf_cached_above(class, wait) &&
				    m_infree(class) < (m_total(class) >> 5)) {
					(void) freelist_populate(class, 1,
					    M_DONTWAIT);
				}
				break;
			}
		} else {
			VERIFY(m_infree(class) == 0 || class == MC_CL);

			(void) freelist_populate(class, 1,
			    (wait & MCR_NOSLEEP) ? M_DONTWAIT : M_WAIT);

			if (m_infree(class) > 0) {
				continue;
			}

			/* Check if there's anything at the cache layer */
			if (mbuf_cached_above(class, wait)) {
				break;
			}

			/* watchdog checkpoint */
			mbuf_watchdog();

			/* We have nothing and cannot block; give up */
			if (wait & MCR_NOSLEEP) {
				if (!(wait & MCR_TRYHARD)) {
					m_fail_cnt(class)++;
					mbstat.m_drops++;
					break;
				}
			}

			/*
			 * If the freelist is still empty and the caller is
			 * willing to be blocked, sleep on the wait channel
			 * until an element is available.  Otherwise, if
			 * MCR_TRYHARD is set, do our best to satisfy the
			 * request without having to go to sleep.
			 */
			if (mbuf_worker_ready &&
			    mbuf_sleep(class, need, wait)) {
				break;
			}

			LCK_MTX_ASSERT(mbuf_mlock, LCK_MTX_ASSERT_OWNED);
		}
	}

	m_alloc_cnt(class) += num - need;
	lck_mtx_unlock(mbuf_mlock);

	return num - need;
}

/*
 * Common de-allocator for rudimentary objects called by the CPU cache
 * layer when one or more elements need to be returned to the appropriate
 * global freelist.
 */
static void
mbuf_slab_free(void *arg, mcache_obj_t *list, __unused int purged)
{
	mbuf_class_t class = (mbuf_class_t)arg;
	mcache_obj_t *nlist;
	unsigned int num = 0;
	int w;

	ASSERT(MBUF_CLASS_VALID(class) && !MBUF_CLASS_COMPOSITE(class));

	lck_mtx_lock(mbuf_mlock);

	for (;;) {
		nlist = list->obj_next;
		list->obj_next = NULL;
		slab_free(class, list);
		++num;
		if ((list = nlist) == NULL) {
			break;
		}
	}
	m_free_cnt(class) += num;

	if ((w = mb_waiters) > 0) {
		mb_waiters = 0;
	}
	if (w) {
		mbwdog_logger("waking up all threads");
	}
	lck_mtx_unlock(mbuf_mlock);

	if (w != 0) {
		wakeup(mb_waitchan);
	}
}

/*
 * Common auditor for rudimentary objects called by the CPU cache layer
 * during an allocation or free request.  For the former, this is called
 * after the objects are obtained from either the bucket or slab layer
 * and before they are returned to the caller.  For the latter, this is
 * called immediately during free and before placing the objects into
 * the bucket or slab layer.
 */
static void
mbuf_slab_audit(void *arg, mcache_obj_t *list, boolean_t alloc)
{
	mbuf_class_t class = (mbuf_class_t)arg;
	mcache_audit_t *mca;

	ASSERT(MBUF_CLASS_VALID(class) && !MBUF_CLASS_COMPOSITE(class));

	while (list != NULL) {
		lck_mtx_lock(mbuf_mlock);
		mca = mcl_audit_buf2mca(class, list);

		/* Do the sanity checks */
		if (class == MC_MBUF) {
			mcl_audit_mbuf(mca, list, FALSE, alloc);
			ASSERT(mca->mca_uflags & MB_SCVALID);
		} else {
			mcl_audit_cluster(mca, list, m_maxsize(class),
			    alloc, TRUE);
			ASSERT(!(mca->mca_uflags & MB_SCVALID));
		}
		/* Record this transaction */
		if (mcltrace) {
			mcache_buffer_log(mca, list, m_cache(class), &mb_start);
		}

		if (alloc) {
			mca->mca_uflags |= MB_INUSE;
		} else {
			mca->mca_uflags &= ~MB_INUSE;
		}
		/* Unpair the object (unconditionally) */
		mca->mca_uptr = NULL;
		lck_mtx_unlock(mbuf_mlock);

		list = list->obj_next;
	}
}

/*
 * Common notify routine for all caches.  It is called by mcache when
 * one or more objects get freed.  We use this indication to trigger
 * the wakeup of any sleeping threads so that they can retry their
 * allocation requests.
 */
static void
mbuf_slab_notify(void *arg, u_int32_t reason)
{
	mbuf_class_t class = (mbuf_class_t)arg;
	int w;

	ASSERT(MBUF_CLASS_VALID(class));

	if (reason != MCN_RETRYALLOC) {
		return;
	}

	lck_mtx_lock(mbuf_mlock);
	if ((w = mb_waiters) > 0) {
		m_notified(class)++;
		mb_waiters = 0;
	}
	if (w) {
		mbwdog_logger("waking up all threads");
	}
	lck_mtx_unlock(mbuf_mlock);

	if (w != 0) {
		wakeup(mb_waitchan);
	}
}

/*
 * Obtain object(s) from the composite class's freelist.
 */
static unsigned int
cslab_alloc(mbuf_class_t class, mcache_obj_t ***plist, unsigned int num)
{
	unsigned int need = num;
	mcl_slab_t *sp, *clsp, *nsp;
	struct mbuf *m;
	mcache_obj_t **list = *plist;
	void *cl;

	VERIFY(need > 0);
	LCK_MTX_ASSERT(mbuf_mlock, LCK_MTX_ASSERT_OWNED);

	/* Get what we can from the freelist */
	while ((*list = m_cobjlist(class)) != NULL) {
		MRANGE(*list);

		m = (struct mbuf *)*list;
		sp = slab_get(m);
		cl = m->m_ext.ext_buf;
		clsp = slab_get(cl);
		VERIFY(m->m_flags == M_EXT && cl != NULL);
		VERIFY(m_get_rfa(m) != NULL && MBUF_IS_COMPOSITE(m));

		if (class == MC_MBUF_CL) {
			VERIFY(clsp->sl_refcnt >= 1 &&
			    clsp->sl_refcnt <= NCLPG);
		} else {
			VERIFY(clsp->sl_refcnt >= 1 &&
			    clsp->sl_refcnt <= NBCLPG);
		}

		if (class == MC_MBUF_16KCL) {
			int k;
			for (nsp = clsp, k = 1; k < NSLABSP16KB; k++) {
				nsp = nsp->sl_next;
				/* Next slab must already be present */
				VERIFY(nsp != NULL);
				VERIFY(nsp->sl_refcnt == 1);
			}
		}

		if ((m_cobjlist(class) = (*list)->obj_next) != NULL &&
		    !MBUF_IN_MAP(m_cobjlist(class))) {
			slab_nextptr_panic(sp, m_cobjlist(class));
			/* NOTREACHED */
		}
		(*list)->obj_next = NULL;
		list = *plist = &(*list)->obj_next;

		if (--need == 0) {
			break;
		}
	}
	m_infree(class) -= (num - need);

	return num - need;
}

/*
 * Place object(s) back into a composite class's freelist.
 */
static unsigned int
cslab_free(mbuf_class_t class, mcache_obj_t *list, int purged)
{
	mcache_obj_t *o, *tail;
	unsigned int num = 0;
	struct mbuf *m, *ms;
	mcache_audit_t *mca = NULL;
	mcache_obj_t *ref_list = NULL;
	mcl_slab_t *clsp, *nsp;
	void *cl;
	mbuf_class_t cl_class;

	ASSERT(MBUF_CLASS_VALID(class) && MBUF_CLASS_COMPOSITE(class));
	LCK_MTX_ASSERT(mbuf_mlock, LCK_MTX_ASSERT_OWNED);

	if (class == MC_MBUF_CL) {
		cl_class = MC_CL;
	} else if (class == MC_MBUF_BIGCL) {
		cl_class = MC_BIGCL;
	} else {
		VERIFY(class == MC_MBUF_16KCL);
		cl_class = MC_16KCL;
	}

	o = tail = list;

	while ((m = ms = (struct mbuf *)o) != NULL) {
		mcache_obj_t *rfa, *nexto = o->obj_next;

		/* Do the mbuf sanity checks */
		if (mclaudit != NULL) {
			mca = mcl_audit_buf2mca(MC_MBUF, (mcache_obj_t *)m);
			if (mclverify) {
				mcache_audit_free_verify(mca, m, 0,
				    m_maxsize(MC_MBUF));
			}
			ms = MCA_SAVED_MBUF_PTR(mca);
		}

		/* Do the cluster sanity checks */
		cl = ms->m_ext.ext_buf;
		clsp = slab_get(cl);
		if (mclverify) {
			size_t size = m_maxsize(cl_class);
			mcache_audit_free_verify(mcl_audit_buf2mca(cl_class,
			    (mcache_obj_t *)cl), cl, 0, size);
		}
		VERIFY(ms->m_type == MT_FREE);
		VERIFY(ms->m_flags == M_EXT);
		VERIFY(m_get_rfa(ms) != NULL && MBUF_IS_COMPOSITE(ms));
		if (cl_class == MC_CL) {
			VERIFY(clsp->sl_refcnt >= 1 &&
			    clsp->sl_refcnt <= NCLPG);
		} else {
			VERIFY(clsp->sl_refcnt >= 1 &&
			    clsp->sl_refcnt <= NBCLPG);
		}
		if (cl_class == MC_16KCL) {
			int k;
			for (nsp = clsp, k = 1; k < NSLABSP16KB; k++) {
				nsp = nsp->sl_next;
				/* Next slab must already be present */
				VERIFY(nsp != NULL);
				VERIFY(nsp->sl_refcnt == 1);
			}
		}

		/*
		 * If we're asked to purge, restore the actual mbuf using
		 * contents of the shadow structure (if auditing is enabled)
		 * and clear EXTF_COMPOSITE flag from the mbuf, as we are
		 * about to free it and the attached cluster into their caches.
		 */
		if (purged) {
			/* Restore constructed mbuf fields */
			if (mclaudit != NULL) {
				mcl_audit_restore_mbuf(m, mca, TRUE);
			}

			MEXT_MINREF(m) = 0;
			MEXT_REF(m) = 0;
			MEXT_PREF(m) = 0;
			MEXT_FLAGS(m) = 0;
			MEXT_PRIV(m) = 0;
			MEXT_PMBUF(m) = NULL;

			rfa = (mcache_obj_t *)(void *)m_get_rfa(m);
			m_set_ext(m, NULL, NULL, NULL);
			rfa->obj_next = ref_list;
			ref_list = rfa;

			m->m_type = MT_FREE;
			m->m_flags = m->m_len = 0;
			m->m_next = m->m_nextpkt = NULL;

			/* Save mbuf fields and make auditing happy */
			if (mclaudit != NULL) {
				mcl_audit_mbuf(mca, o, FALSE, FALSE);
			}

			VERIFY(m_total(class) > 0);
			m_total(class)--;

			/* Free the mbuf */
			o->obj_next = NULL;
			slab_free(MC_MBUF, o);

			/* And free the cluster */
			((mcache_obj_t *)cl)->obj_next = NULL;
			if (class == MC_MBUF_CL) {
				slab_free(MC_CL, cl);
			} else if (class == MC_MBUF_BIGCL) {
				slab_free(MC_BIGCL, cl);
			} else {
				slab_free(MC_16KCL, cl);
			}
		}

		++num;
		tail = o;
		o = nexto;
	}

	if (!purged) {
		tail->obj_next = m_cobjlist(class);
		m_cobjlist(class) = list;
		m_infree(class) += num;
	} else if (ref_list != NULL) {
		mcache_free_ext(ref_cache, ref_list);
	}

	return num;
}

/*
 * Common allocator for composite objects called by the CPU cache layer
 * during an allocation request whenever there is no available element in
 * the bucket layer.  It returns one or more composite elements from the
 * appropriate global freelist.  If the freelist is empty, it will attempt
 * to obtain the rudimentary objects from their caches and construct them
 * into composite mbuf + cluster objects.
 */
static unsigned int
mbuf_cslab_alloc(void *arg, mcache_obj_t ***plist, unsigned int needed,
    int wait)
{
	mbuf_class_t class = (mbuf_class_t)arg;
	mbuf_class_t cl_class = 0;
	unsigned int num = 0, cnum = 0, want = needed;
	mcache_obj_t *ref_list = NULL;
	mcache_obj_t *mp_list = NULL;
	mcache_obj_t *clp_list = NULL;
	mcache_obj_t **list;
	struct ext_ref *rfa;
	struct mbuf *m;
	void *cl;

	ASSERT(MBUF_CLASS_VALID(class) && MBUF_CLASS_COMPOSITE(class));
	ASSERT(needed > 0);

	/* There should not be any slab for this class */
	VERIFY(m_slab_cnt(class) == 0 &&
	    m_slablist(class).tqh_first == NULL &&
	    m_slablist(class).tqh_last == NULL);

	lck_mtx_lock(mbuf_mlock);

	/* Try using the freelist first */
	num = cslab_alloc(class, plist, needed);
	list = *plist;
	if (num == needed) {
		m_alloc_cnt(class) += num;
		lck_mtx_unlock(mbuf_mlock);
		return needed;
	}

	lck_mtx_unlock(mbuf_mlock);

	/*
	 * We could not satisfy the request using the freelist alone;
	 * allocate from the appropriate rudimentary caches and use
	 * whatever we can get to construct the composite objects.
	 */
	needed -= num;

	/*
	 * Mark these allocation requests as coming from a composite cache.
	 * Also, if the caller is willing to be blocked, mark the request
	 * with MCR_FAILOK such that we don't end up sleeping at the mbuf
	 * slab layer waiting for the individual object when one or more
	 * of the already-constructed composite objects are available.
	 */
	wait |= MCR_COMP;
	if (!(wait & MCR_NOSLEEP)) {
		wait |= MCR_FAILOK;
	}

	/* allocate mbufs */
	needed = mcache_alloc_ext(m_cache(MC_MBUF), &mp_list, needed, wait);
	if (needed == 0) {
		ASSERT(mp_list == NULL);
		goto fail;
	}

	/* allocate clusters */
	if (class == MC_MBUF_CL) {
		cl_class = MC_CL;
	} else if (class == MC_MBUF_BIGCL) {
		cl_class = MC_BIGCL;
	} else {
		VERIFY(class == MC_MBUF_16KCL);
		cl_class = MC_16KCL;
	}
	needed = mcache_alloc_ext(m_cache(cl_class), &clp_list, needed, wait);
	if (needed == 0) {
		ASSERT(clp_list == NULL);
		goto fail;
	}

	needed = mcache_alloc_ext(ref_cache, &ref_list, needed, wait);
	if (needed == 0) {
		ASSERT(ref_list == NULL);
		goto fail;
	}

	/*
	 * By this time "needed" is MIN(mbuf, cluster, ref).  Any left
	 * overs will get freed accordingly before we return to caller.
	 */
	for (cnum = 0; cnum < needed; cnum++) {
		struct mbuf *ms;

		m = ms = (struct mbuf *)mp_list;
		mp_list = mp_list->obj_next;

		cl = clp_list;
		clp_list = clp_list->obj_next;
		((mcache_obj_t *)cl)->obj_next = NULL;

		rfa = (struct ext_ref *)ref_list;
		ref_list = ref_list->obj_next;
		((mcache_obj_t *)(void *)rfa)->obj_next = NULL;

		/*
		 * If auditing is enabled, construct the shadow mbuf
		 * in the audit structure instead of in the actual one.
		 * mbuf_cslab_audit() will take care of restoring the
		 * contents after the integrity check.
		 */
		if (mclaudit != NULL) {
			mcache_audit_t *mca, *cl_mca;

			lck_mtx_lock(mbuf_mlock);
			mca = mcl_audit_buf2mca(MC_MBUF, (mcache_obj_t *)m);
			ms = MCA_SAVED_MBUF_PTR(mca);
			cl_mca = mcl_audit_buf2mca(cl_class,
			    (mcache_obj_t *)cl);

			/*
			 * Pair them up.  Note that this is done at the time
			 * the mbuf+cluster objects are constructed.  This
			 * information should be treated as "best effort"
			 * debugging hint since more than one mbufs can refer
			 * to a cluster.  In that case, the cluster might not
			 * be freed along with the mbuf it was paired with.
			 */
			mca->mca_uptr = cl_mca;
			cl_mca->mca_uptr = mca;

			ASSERT(mca->mca_uflags & MB_SCVALID);
			ASSERT(!(cl_mca->mca_uflags & MB_SCVALID));
			lck_mtx_unlock(mbuf_mlock);

			/* Technically, they are in the freelist */
			if (mclverify) {
				size_t size;

				mcache_set_pattern(MCACHE_FREE_PATTERN, m,
				    m_maxsize(MC_MBUF));

				if (class == MC_MBUF_CL) {
					size = m_maxsize(MC_CL);
				} else if (class == MC_MBUF_BIGCL) {
					size = m_maxsize(MC_BIGCL);
				} else {
					size = m_maxsize(MC_16KCL);
				}

				mcache_set_pattern(MCACHE_FREE_PATTERN, cl,
				    size);
			}
		}

		mbuf_init(ms, 0, MT_FREE);
		if (class == MC_MBUF_16KCL) {
			MBUF_16KCL_INIT(ms, cl, rfa, 0, EXTF_COMPOSITE);
		} else if (class == MC_MBUF_BIGCL) {
			MBUF_BIGCL_INIT(ms, cl, rfa, 0, EXTF_COMPOSITE);
		} else {
			MBUF_CL_INIT(ms, cl, rfa, 0, EXTF_COMPOSITE);
		}
		VERIFY(ms->m_flags == M_EXT);
		VERIFY(m_get_rfa(ms) != NULL && MBUF_IS_COMPOSITE(ms));

		*list = (mcache_obj_t *)m;
		(*list)->obj_next = NULL;
		list = *plist = &(*list)->obj_next;
	}

fail:
	/*
	 * Free up what's left of the above.
	 */
	if (mp_list != NULL) {
		mcache_free_ext(m_cache(MC_MBUF), mp_list);
	}
	if (clp_list != NULL) {
		mcache_free_ext(m_cache(cl_class), clp_list);
	}
	if (ref_list != NULL) {
		mcache_free_ext(ref_cache, ref_list);
	}

	lck_mtx_lock(mbuf_mlock);
	if (num > 0 || cnum > 0) {
		m_total(class) += cnum;
		VERIFY(m_total(class) <= m_maxlimit(class));
		m_alloc_cnt(class) += num + cnum;
	}
	if ((num + cnum) < want) {
		m_fail_cnt(class) += (want - (num + cnum));
	}
	lck_mtx_unlock(mbuf_mlock);

	return num + cnum;
}

/*
 * Common de-allocator for composite objects called by the CPU cache
 * layer when one or more elements need to be returned to the appropriate
 * global freelist.
 */
static void
mbuf_cslab_free(void *arg, mcache_obj_t *list, int purged)
{
	mbuf_class_t class = (mbuf_class_t)arg;
	unsigned int num;
	int w;

	ASSERT(MBUF_CLASS_VALID(class) && MBUF_CLASS_COMPOSITE(class));

	lck_mtx_lock(mbuf_mlock);

	num = cslab_free(class, list, purged);
	m_free_cnt(class) += num;

	if ((w = mb_waiters) > 0) {
		mb_waiters = 0;
	}
	if (w) {
		mbwdog_logger("waking up all threads");
	}

	lck_mtx_unlock(mbuf_mlock);

	if (w != 0) {
		wakeup(mb_waitchan);
	}
}

/*
 * Common auditor for composite objects called by the CPU cache layer
 * during an allocation or free request.  For the former, this is called
 * after the objects are obtained from either the bucket or slab layer
 * and before they are returned to the caller.  For the latter, this is
 * called immediately during free and before placing the objects into
 * the bucket or slab layer.
 */
static void
mbuf_cslab_audit(void *arg, mcache_obj_t *list, boolean_t alloc)
{
	mbuf_class_t class = (mbuf_class_t)arg, cl_class;
	mcache_audit_t *mca;
	struct mbuf *m, *ms;
	mcl_slab_t *clsp, *nsp;
	size_t cl_size;
	void *cl;

	ASSERT(MBUF_CLASS_VALID(class) && MBUF_CLASS_COMPOSITE(class));
	if (class == MC_MBUF_CL) {
		cl_class = MC_CL;
	} else if (class == MC_MBUF_BIGCL) {
		cl_class = MC_BIGCL;
	} else {
		cl_class = MC_16KCL;
	}
	cl_size = m_maxsize(cl_class);

	while ((m = ms = (struct mbuf *)list) != NULL) {
		lck_mtx_lock(mbuf_mlock);
		/* Do the mbuf sanity checks and record its transaction */
		mca = mcl_audit_buf2mca(MC_MBUF, (mcache_obj_t *)m);
		mcl_audit_mbuf(mca, m, TRUE, alloc);
		if (mcltrace) {
			mcache_buffer_log(mca, m, m_cache(class), &mb_start);
		}

		if (alloc) {
			mca->mca_uflags |= MB_COMP_INUSE;
		} else {
			mca->mca_uflags &= ~MB_COMP_INUSE;
		}

		/*
		 * Use the shadow mbuf in the audit structure if we are
		 * freeing, since the contents of the actual mbuf has been
		 * pattern-filled by the above call to mcl_audit_mbuf().
		 */
		if (!alloc && mclverify) {
			ms = MCA_SAVED_MBUF_PTR(mca);
		}

		/* Do the cluster sanity checks and record its transaction */
		cl = ms->m_ext.ext_buf;
		clsp = slab_get(cl);
		VERIFY(ms->m_flags == M_EXT && cl != NULL);
		VERIFY(m_get_rfa(ms) != NULL && MBUF_IS_COMPOSITE(ms));
		if (class == MC_MBUF_CL) {
			VERIFY(clsp->sl_refcnt >= 1 &&
			    clsp->sl_refcnt <= NCLPG);
		} else {
			VERIFY(clsp->sl_refcnt >= 1 &&
			    clsp->sl_refcnt <= NBCLPG);
		}

		if (class == MC_MBUF_16KCL) {
			int k;
			for (nsp = clsp, k = 1; k < NSLABSP16KB; k++) {
				nsp = nsp->sl_next;
				/* Next slab must already be present */
				VERIFY(nsp != NULL);
				VERIFY(nsp->sl_refcnt == 1);
			}
		}


		mca = mcl_audit_buf2mca(cl_class, cl);
		mcl_audit_cluster(mca, cl, cl_size, alloc, FALSE);
		if (mcltrace) {
			mcache_buffer_log(mca, cl, m_cache(class), &mb_start);
		}

		if (alloc) {
			mca->mca_uflags |= MB_COMP_INUSE;
		} else {
			mca->mca_uflags &= ~MB_COMP_INUSE;
		}
		lck_mtx_unlock(mbuf_mlock);

		list = list->obj_next;
	}
}

static void
m_vm_error_stats(uint32_t *cnt, uint64_t *ts, uint64_t *size,
    uint64_t alloc_size, kern_return_t error)
{
	*cnt = *cnt + 1;
	*ts = net_uptime();
	if (size) {
		*size = alloc_size;
	}
	switch (error) {
	case KERN_SUCCESS:
		break;
	case KERN_INVALID_ARGUMENT:
		mb_kmem_stats[0]++;
		break;
	case KERN_INVALID_ADDRESS:
		mb_kmem_stats[1]++;
		break;
	case KERN_RESOURCE_SHORTAGE:
		mb_kmem_stats[2]++;
		break;
	case KERN_NO_SPACE:
		mb_kmem_stats[3]++;
		break;
	case KERN_FAILURE:
		mb_kmem_stats[4]++;
		break;
	default:
		mb_kmem_stats[5]++;
		break;
	}
}

static vm_offset_t
kmem_mb_alloc(vm_map_t mbmap, int size, int physContig, kern_return_t *err)
{
	vm_offset_t addr = 0;
	kern_return_t kr = KERN_SUCCESS;

	if (!physContig) {
		kr = kmem_alloc(mbmap, &addr, size,
		    KMA_KOBJECT | KMA_LOMEM, VM_KERN_MEMORY_MBUF);
	} else {
		kr = kmem_alloc_contig(mbmap, &addr, size, PAGE_MASK, 0xfffff,
		    0, KMA_KOBJECT | KMA_LOMEM, VM_KERN_MEMORY_MBUF);
	}

	if (kr != KERN_SUCCESS) {
		addr = 0;
	}
	if (err) {
		*err = kr;
	}

	return addr;
}

/*
 * Allocate some number of mbuf clusters and place on cluster freelist.
 */
static int
m_clalloc(const u_int32_t num, const int wait, const u_int32_t bufsize)
{
	int i, count = 0;
	vm_size_t size = 0;
	int numpages = 0, large_buffer;
	vm_offset_t page = 0;
	mcache_audit_t *mca_list = NULL;
	mcache_obj_t *con_list = NULL;
	mcl_slab_t *sp;
	mbuf_class_t class;
	kern_return_t error;

	/* Set if a buffer allocation needs allocation of multiple pages */
	large_buffer = ((bufsize == m_maxsize(MC_16KCL)) &&
	    PAGE_SIZE < M16KCLBYTES);
	VERIFY(bufsize == m_maxsize(MC_BIGCL) ||
	    bufsize == m_maxsize(MC_16KCL));

	VERIFY((bufsize == PAGE_SIZE) ||
	    (bufsize > PAGE_SIZE && bufsize == m_maxsize(MC_16KCL)));

	if (bufsize == m_size(MC_BIGCL)) {
		class = MC_BIGCL;
	} else {
		class = MC_16KCL;
	}

	LCK_MTX_ASSERT(mbuf_mlock, LCK_MTX_ASSERT_OWNED);

	/*
	 * Multiple threads may attempt to populate the cluster map one
	 * after another.  Since we drop the lock below prior to acquiring
	 * the physical page(s), our view of the cluster map may no longer
	 * be accurate, and we could end up over-committing the pages beyond
	 * the maximum allowed for each class.  To prevent it, this entire
	 * operation (including the page mapping) is serialized.
	 */
	while (mb_clalloc_busy) {
		mb_clalloc_waiters++;
		(void) msleep(mb_clalloc_waitchan, mbuf_mlock,
		    (PZERO - 1), "m_clalloc", NULL);
		LCK_MTX_ASSERT(mbuf_mlock, LCK_MTX_ASSERT_OWNED);
	}

	/* We are busy now; tell everyone else to go away */
	mb_clalloc_busy = TRUE;

	/*
	 * Honor the caller's wish to block or not block.  We have a way
	 * to grow the pool asynchronously using the mbuf worker thread.
	 */
	i = m_howmany(num, bufsize);
	if (i <= 0 || (wait & M_DONTWAIT)) {
		goto out;
	}

	lck_mtx_unlock(mbuf_mlock);

	size = round_page(i * bufsize);
	page = kmem_mb_alloc(mb_map, size, large_buffer, &error);

	/*
	 * If we did ask for "n" 16KB physically contiguous chunks
	 * and didn't get them, then please try again without this
	 * restriction.
	 */
	net_update_uptime();
	if (large_buffer && page == 0) {
		m_vm_error_stats(&mb_kmem_contig_failed,
		    &mb_kmem_contig_failed_ts,
		    &mb_kmem_contig_failed_size,
		    size, error);
		page = kmem_mb_alloc(mb_map, size, 0, &error);
	}

	if (page == 0) {
		m_vm_error_stats(&mb_kmem_failed,
		    &mb_kmem_failed_ts,
		    &mb_kmem_failed_size,
		    size, error);
#if PAGE_SIZE == 4096
		if (bufsize == m_maxsize(MC_BIGCL)) {
#else
		if (bufsize >= m_maxsize(MC_BIGCL)) {
#endif
			/* Try for 1 page if failed */
			size = PAGE_SIZE;
			page = kmem_mb_alloc(mb_map, size, 0, &error);
			if (page == 0) {
				m_vm_error_stats(&mb_kmem_one_failed,
				    &mb_kmem_one_failed_ts,
				    NULL, size, error);
			}
		}

		if (page == 0) {
			lck_mtx_lock(mbuf_mlock);
			goto out;
		}
	}

	VERIFY(IS_P2ALIGNED(page, PAGE_SIZE));
	numpages = size / PAGE_SIZE;

	/* If auditing is enabled, allocate the audit structures now */
	if (mclaudit != NULL) {
		int needed;

		/*
		 * Yes, I realize this is a waste of memory for clusters
		 * that never get transformed into mbufs, as we may end
		 * up with NMBPG-1 unused audit structures per cluster.
		 * But doing so tremendously simplifies the allocation
		 * strategy, since at this point we are not holding the
		 * mbuf lock and the caller is okay to be blocked.
		 */
		if (bufsize == PAGE_SIZE) {
			needed = numpages * NMBPG;

			i = mcache_alloc_ext(mcl_audit_con_cache,
			    &con_list, needed, MCR_SLEEP);

			VERIFY(con_list != NULL && i == needed);
		} else {
			/*
			 * if multiple 4K pages are being used for a
			 * 16K cluster
			 */
			needed = numpages / NSLABSP16KB;
		}

		i = mcache_alloc_ext(mcache_audit_cache,
		    (mcache_obj_t **)&mca_list, needed, MCR_SLEEP);

		VERIFY(mca_list != NULL && i == needed);
	}

	lck_mtx_lock(mbuf_mlock);

	for (i = 0; i < numpages; i++, page += PAGE_SIZE) {
		ppnum_t offset =
		    ((unsigned char *)page - mbutl) >> PAGE_SHIFT;
		ppnum_t new_page = pmap_find_phys(kernel_pmap, page);

		/*
		 * If there is a mapper the appropriate I/O page is
		 * returned; zero out the page to discard its past
		 * contents to prevent exposing leftover kernel memory.
		 */
		VERIFY(offset < mcl_pages);
		if (mcl_paddr_base != 0) {
			bzero((void *)(uintptr_t) page, PAGE_SIZE);
			new_page = IOMapperInsertPage(mcl_paddr_base,
			    offset, new_page);
		}
		mcl_paddr[offset] = new_page;

		/* Pattern-fill this fresh page */
		if (mclverify) {
			mcache_set_pattern(MCACHE_FREE_PATTERN,
			    (caddr_t)page, PAGE_SIZE);
		}
		if (bufsize == PAGE_SIZE) {
			mcache_obj_t *buf;
			/* One for the entire page */
			sp = slab_get((void *)page);
			if (mclaudit != NULL) {
				mcl_audit_init((void *)page,
				    &mca_list, &con_list,
				    AUDIT_CONTENTS_SIZE, NMBPG);
			}
			VERIFY(sp->sl_refcnt == 0 && sp->sl_flags == 0);
			slab_init(sp, class, SLF_MAPPED, (void *)page,
			    (void *)page, PAGE_SIZE, 0, 1);
			buf = (mcache_obj_t *)page;
			buf->obj_next = NULL;

			/* Insert this slab */
			slab_insert(sp, class);

			/* Update stats now since slab_get drops the lock */
			++m_infree(class);
			++m_total(class);
			VERIFY(m_total(class) <= m_maxlimit(class));
			if (class == MC_BIGCL) {
				mbstat.m_bigclfree = m_infree(MC_BIGCL) +
				    m_infree(MC_MBUF_BIGCL);
				mbstat.m_bigclusters = m_total(MC_BIGCL);
			}
			++count;
		} else if ((bufsize > PAGE_SIZE) &&
		    (i % NSLABSP16KB) == 0) {
			union m16kcluster *m16kcl = (union m16kcluster *)page;
			mcl_slab_t *nsp;
			int k;

			/* One for the entire 16KB */
			sp = slab_get(m16kcl);
			if (mclaudit != NULL) {
				mcl_audit_init(m16kcl, &mca_list, NULL, 0, 1);
			}

			VERIFY(sp->sl_refcnt == 0 && sp->sl_flags == 0);
			slab_init(sp, MC_16KCL, SLF_MAPPED,
			    m16kcl, m16kcl, bufsize, 0, 1);
			m16kcl->m16kcl_next = NULL;

			/*
			 * 2nd-Nth page's slab is part of the first one,
			 * where N is NSLABSP16KB.
			 */
			for (k = 1; k < NSLABSP16KB; k++) {
				nsp = slab_get(((union mbigcluster *)page) + k);
				VERIFY(nsp->sl_refcnt == 0 &&
				    nsp->sl_flags == 0);
				slab_init(nsp, MC_16KCL,
				    SLF_MAPPED | SLF_PARTIAL,
				    m16kcl, NULL, 0, 0, 0);
			}
			/* Insert this slab */
			slab_insert(sp, MC_16KCL);

			/* Update stats now since slab_get drops the lock */
			++m_infree(MC_16KCL);
			++m_total(MC_16KCL);
			VERIFY(m_total(MC_16KCL) <= m_maxlimit(MC_16KCL));
			++count;
		}
	}
	VERIFY(mca_list == NULL && con_list == NULL);

	/* We're done; let others enter */
	mb_clalloc_busy = FALSE;
	if (mb_clalloc_waiters > 0) {
		mb_clalloc_waiters = 0;
		wakeup(mb_clalloc_waitchan);
	}

	return count;
out:
	LCK_MTX_ASSERT(mbuf_mlock, LCK_MTX_ASSERT_OWNED);

	mtracelarge_register(size);

	/* We're done; let others enter */
	mb_clalloc_busy = FALSE;
	if (mb_clalloc_waiters > 0) {
		mb_clalloc_waiters = 0;
		wakeup(mb_clalloc_waitchan);
	}

	/*
	 * When non-blocking we kick a thread if we have to grow the
	 * pool or if the number of free clusters is less than requested.
	 */
	if (i > 0 && mbuf_worker_ready && mbuf_worker_needs_wakeup) {
		mbwdog_logger("waking up the worker thread to to grow %s by %d",
		    m_cname(class), i);
		wakeup((caddr_t)&mbuf_worker_needs_wakeup);
		mbuf_worker_needs_wakeup = FALSE;
	}
	if (class == MC_BIGCL) {
		if (i > 0) {
			/*
			 * Remember total number of 4KB clusters needed
			 * at this time.
			 */
			i += m_total(MC_BIGCL);
			if (i > m_region_expand(MC_BIGCL)) {
				m_region_expand(MC_BIGCL) = i;
			}
		}
		if (m_infree(MC_BIGCL) >= num) {
			return 1;
		}
	} else {
		if (i > 0) {
			/*
			 * Remember total number of 16KB clusters needed
			 * at this time.
			 */
			i += m_total(MC_16KCL);
			if (i > m_region_expand(MC_16KCL)) {
				m_region_expand(MC_16KCL) = i;
			}
		}
		if (m_infree(MC_16KCL) >= num) {
			return 1;
		}
	}
	return 0;
}

/*
 * Populate the global freelist of the corresponding buffer class.
 */
static int
freelist_populate(mbuf_class_t class, unsigned int num, int wait)
{
	mcache_obj_t *o = NULL;
	int i, numpages = 0, count;
	mbuf_class_t super_class;

	VERIFY(class == MC_MBUF || class == MC_CL || class == MC_BIGCL ||
	    class == MC_16KCL);

	LCK_MTX_ASSERT(mbuf_mlock, LCK_MTX_ASSERT_OWNED);

	VERIFY(PAGE_SIZE == m_maxsize(MC_BIGCL) ||
	    PAGE_SIZE == m_maxsize(MC_16KCL));

	if (m_maxsize(class) >= PAGE_SIZE) {
		return m_clalloc(num, wait, m_maxsize(class)) != 0;
	}

	/*
	 * The rest of the function will allocate pages and will slice
	 * them up into the right size
	 */

	numpages = (num * m_size(class) + PAGE_SIZE - 1) / PAGE_SIZE;

	/* Currently assume that pages are 4K or 16K */
	if (PAGE_SIZE == m_maxsize(MC_BIGCL)) {
		super_class = MC_BIGCL;
	} else {
		super_class = MC_16KCL;
	}

	i = m_clalloc(numpages, wait, m_maxsize(super_class));

	/* how many objects will we cut the page into? */
	int numobj = PAGE_SIZE / m_maxsize(class);

	for (count = 0; count < numpages; count++) {
		/* respect totals, minlimit, maxlimit */
		if (m_total(super_class) <= m_minlimit(super_class) ||
		    m_total(class) >= m_maxlimit(class)) {
			break;
		}

		if ((o = slab_alloc(super_class, wait)) == NULL) {
			break;
		}

		struct mbuf *m = (struct mbuf *)o;
		union mcluster *c = (union mcluster *)o;
		union mbigcluster *mbc = (union mbigcluster *)o;
		mcl_slab_t *sp = slab_get(o);
		mcache_audit_t *mca = NULL;

		/*
		 * since one full page will be converted to MC_MBUF or
		 * MC_CL, verify that the reference count will match that
		 * assumption
		 */
		VERIFY(sp->sl_refcnt == 1 && slab_is_detached(sp));
		VERIFY((sp->sl_flags & (SLF_MAPPED | SLF_PARTIAL)) == SLF_MAPPED);
		/*
		 * Make sure that the cluster is unmolested
		 * while in freelist
		 */
		if (mclverify) {
			mca = mcl_audit_buf2mca(super_class,
			    (mcache_obj_t *)o);
			mcache_audit_free_verify(mca,
			    (mcache_obj_t *)o, 0, m_maxsize(super_class));
		}

		/* Reinitialize it as an mbuf or 2K or 4K slab */
		slab_init(sp, class, sp->sl_flags,
		    sp->sl_base, NULL, PAGE_SIZE, 0, numobj);

		VERIFY(sp->sl_head == NULL);

		VERIFY(m_total(super_class) >= 1);
		m_total(super_class)--;

		if (super_class == MC_BIGCL) {
			mbstat.m_bigclusters = m_total(MC_BIGCL);
		}

		m_total(class) += numobj;
		VERIFY(m_total(class) <= m_maxlimit(class));
		m_infree(class) += numobj;

		i = numobj;
		if (class == MC_MBUF) {
			mbstat.m_mbufs = m_total(MC_MBUF);
			mtype_stat_add(MT_FREE, NMBPG);
			while (i--) {
				/*
				 * If auditing is enabled, construct the
				 * shadow mbuf in the audit structure
				 * instead of the actual one.
				 * mbuf_slab_audit() will take care of
				 * restoring the contents after the
				 * integrity check.
				 */
				if (mclaudit != NULL) {
					struct mbuf *ms;
					mca = mcl_audit_buf2mca(MC_MBUF,
					    (mcache_obj_t *)m);
					ms = MCA_SAVED_MBUF_PTR(mca);
					ms->m_type = MT_FREE;
				} else {
					m->m_type = MT_FREE;
				}
				m->m_next = sp->sl_head;
				sp->sl_head = (void *)m++;
			}
		} else if (class == MC_CL) { /* MC_CL */
			mbstat.m_clfree =
			    m_infree(MC_CL) + m_infree(MC_MBUF_CL);
			mbstat.m_clusters = m_total(MC_CL);
			while (i--) {
				c->mcl_next = sp->sl_head;
				sp->sl_head = (void *)c++;
			}
		} else {
			VERIFY(class == MC_BIGCL);
			mbstat.m_bigclusters = m_total(MC_BIGCL);
			mbstat.m_bigclfree = m_infree(MC_BIGCL) +
			    m_infree(MC_MBUF_BIGCL);
			while (i--) {
				mbc->mbc_next = sp->sl_head;
				sp->sl_head = (void *)mbc++;
			}
		}

		/* Insert into the mbuf or 2k or 4k slab list */
		slab_insert(sp, class);

		if ((i = mb_waiters) > 0) {
			mb_waiters = 0;
		}
		if (i != 0) {
			mbwdog_logger("waking up all threads");
			wakeup(mb_waitchan);
		}
	}
	return count != 0;
}

/*
 * For each class, initialize the freelist to hold m_minlimit() objects.
 */
static void
freelist_init(mbuf_class_t class)
{
	LCK_MTX_ASSERT(mbuf_mlock, LCK_MTX_ASSERT_OWNED);

	VERIFY(class == MC_CL || class == MC_BIGCL);
	VERIFY(m_total(class) == 0);
	VERIFY(m_minlimit(class) > 0);

	while (m_total(class) < m_minlimit(class)) {
		(void) freelist_populate(class, m_minlimit(class), M_WAIT);
	}

	VERIFY(m_total(class) >= m_minlimit(class));
}

/*
 * (Inaccurately) check if it might be worth a trip back to the
 * mcache layer due the availability of objects there.  We'll
 * end up back here if there's nothing up there.
 */
static boolean_t
mbuf_cached_above(mbuf_class_t class, int wait)
{
	switch (class) {
	case MC_MBUF:
		if (wait & MCR_COMP) {
			return !mcache_bkt_isempty(m_cache(MC_MBUF_CL)) ||
			       !mcache_bkt_isempty(m_cache(MC_MBUF_BIGCL));
		}
		break;

	case MC_CL:
		if (wait & MCR_COMP) {
			return !mcache_bkt_isempty(m_cache(MC_MBUF_CL));
		}
		break;

	case MC_BIGCL:
		if (wait & MCR_COMP) {
			return !mcache_bkt_isempty(m_cache(MC_MBUF_BIGCL));
		}
		break;

	case MC_16KCL:
		if (wait & MCR_COMP) {
			return !mcache_bkt_isempty(m_cache(MC_MBUF_16KCL));
		}
		break;

	case MC_MBUF_CL:
	case MC_MBUF_BIGCL:
	case MC_MBUF_16KCL:
		break;

	default:
		VERIFY(0);
		/* NOTREACHED */
	}

	return !mcache_bkt_isempty(m_cache(class));
}

/*
 * If possible, convert constructed objects to raw ones.
 */
static boolean_t
mbuf_steal(mbuf_class_t class, unsigned int num)
{
	mcache_obj_t *top = NULL;
	mcache_obj_t **list = &top;
	unsigned int tot = 0;

	LCK_MTX_ASSERT(mbuf_mlock, LCK_MTX_ASSERT_OWNED);

	switch (class) {
	case MC_MBUF:
	case MC_CL:
	case MC_BIGCL:
	case MC_16KCL:
		return FALSE;

	case MC_MBUF_CL:
	case MC_MBUF_BIGCL:
	case MC_MBUF_16KCL:
		/* Get the required number of constructed objects if possible */
		if (m_infree(class) > m_minlimit(class)) {
			tot = cslab_alloc(class, &list,
			    MIN(num, m_infree(class)));
		}

		/* And destroy them to get back the raw objects */
		if (top != NULL) {
			(void) cslab_free(class, top, 1);
		}
		break;

	default:
		VERIFY(0);
		/* NOTREACHED */
	}

	return tot == num;
}

static void
m_reclaim(mbuf_class_t class, unsigned int num, boolean_t comp)
{
	int m, bmap = 0;

	LCK_MTX_ASSERT(mbuf_mlock, LCK_MTX_ASSERT_OWNED);

	VERIFY(m_total(MC_CL) <= m_maxlimit(MC_CL));
	VERIFY(m_total(MC_BIGCL) <= m_maxlimit(MC_BIGCL));
	VERIFY(m_total(MC_16KCL) <= m_maxlimit(MC_16KCL));

	/*
	 * This logic can be made smarter; for now, simply mark
	 * all other related classes as potential victims.
	 */
	switch (class) {
	case MC_MBUF:
		m_wantpurge(MC_CL)++;
		m_wantpurge(MC_BIGCL)++;
		m_wantpurge(MC_MBUF_CL)++;
		m_wantpurge(MC_MBUF_BIGCL)++;
		break;

	case MC_CL:
		m_wantpurge(MC_MBUF)++;
		m_wantpurge(MC_BIGCL)++;
		m_wantpurge(MC_MBUF_BIGCL)++;
		if (!comp) {
			m_wantpurge(MC_MBUF_CL)++;
		}
		break;

	case MC_BIGCL:
		m_wantpurge(MC_MBUF)++;
		m_wantpurge(MC_CL)++;
		m_wantpurge(MC_MBUF_CL)++;
		if (!comp) {
			m_wantpurge(MC_MBUF_BIGCL)++;
		}
		break;

	case MC_16KCL:
		if (!comp) {
			m_wantpurge(MC_MBUF_16KCL)++;
		}
		break;

	default:
		VERIFY(0);
		/* NOTREACHED */
	}

	/*
	 * Run through each marked class and check if we really need to
	 * purge (and therefore temporarily disable) the per-CPU caches
	 * layer used by the class.  If so, remember the classes since
	 * we are going to drop the lock below prior to purging.
	 */
	for (m = 0; m < MC_MAX; m++) {
		if (m_wantpurge(m) > 0) {
			m_wantpurge(m) = 0;
			/*
			 * Try hard to steal the required number of objects
			 * from the freelist of other mbuf classes.  Only
			 * purge and disable the per-CPU caches layer when
			 * we don't have enough; it's the last resort.
			 */
			if (!mbuf_steal(m, num)) {
				bmap |= (1 << m);
			}
		}
	}

	lck_mtx_unlock(mbuf_mlock);

	if (bmap != 0) {
		/* signal the domains to drain */
		net_drain_domains();

		/* Sigh; we have no other choices but to ask mcache to purge */
		for (m = 0; m < MC_MAX; m++) {
			if ((bmap & (1 << m)) &&
			    mcache_purge_cache(m_cache(m), TRUE)) {
				lck_mtx_lock(mbuf_mlock);
				m_purge_cnt(m)++;
				mbstat.m_drain++;
				lck_mtx_unlock(mbuf_mlock);
			}
		}
	} else {
		/*
		 * Request mcache to reap extra elements from all of its caches;
		 * note that all reaps are serialized and happen only at a fixed
		 * interval.
		 */
		mcache_reap();
	}
	lck_mtx_lock(mbuf_mlock);
}

struct mbuf *
m_get_common(int wait, short type, int hdr)
{
	struct mbuf *m;

	int mcflags = MSLEEPF(wait);

	/* Is this due to a non-blocking retry?  If so, then try harder */
	if (mcflags & MCR_NOSLEEP) {
		mcflags |= MCR_TRYHARD;
	}

	m = mcache_alloc(m_cache(MC_MBUF), mcflags);
	if (m != NULL) {
		mbuf_init(m, hdr, type);
		mtype_stat_inc(type);
		mtype_stat_dec(MT_FREE);
	}
	return m;
}

/*
 * Space allocation routines; these are also available as macros
 * for critical paths.
 */
#define _M_GETHDR(wait, type)   m_get_common(wait, type, 1)

struct mbuf *
m_free(struct mbuf *m)
{
	struct mbuf *n = m->m_next;

	if (m->m_type == MT_FREE) {
		panic("m_free: freeing an already freed mbuf");
	}

	if (m->m_flags & M_PKTHDR) {
		/* Free the aux data and tags if there is any */
		m_tag_delete_chain(m);

		m_do_tx_compl_callback(m, NULL);
	}

	if (m->m_flags & M_EXT) {
		if (MBUF_IS_PAIRED(m) && m_free_paired(m)) {
			return n;
		}
		/*
		 * Make sure that we don't touch any ext_ref
		 * member after we decrement the reference count
		 * since that may lead to use-after-free
		 * when we do not hold the last reference.
		 */
		const bool composite = !!(MEXT_FLAGS(m) & EXTF_COMPOSITE);
		const m_ext_free_func_t m_free_func = m_get_ext_free(m);
		const uint16_t minref = MEXT_MINREF(m);
		const uint16_t refcnt = m_decref(m);

		if (refcnt == minref && !composite) {
			if (m_free_func == NULL) {
				mcache_free(m_cache(MC_CL), m->m_ext.ext_buf);
			} else if (m_free_func == m_bigfree) {
				mcache_free(m_cache(MC_BIGCL),
				    m->m_ext.ext_buf);
			} else if (m_free_func == m_16kfree) {
				mcache_free(m_cache(MC_16KCL),
				    m->m_ext.ext_buf);
			} else {
				(*m_free_func)(m->m_ext.ext_buf,
				    m->m_ext.ext_size, m_get_ext_arg(m));
			}
			mcache_free(ref_cache, m_get_rfa(m));
			m_set_ext(m, NULL, NULL, NULL);
		} else if (refcnt == minref && composite) {
			VERIFY(!(MEXT_FLAGS(m) & EXTF_PAIRED));

			mtype_stat_dec(m->m_type);
			mtype_stat_inc(MT_FREE);

			m->m_type = MT_FREE;
			m->m_flags = M_EXT;
			m->m_len = 0;
			m->m_next = m->m_nextpkt = NULL;
			/*
			 * MEXT_FLAGS is safe to access here
			 * since we are now sure that we held
			 * the last reference to ext_ref.
			 */
			MEXT_FLAGS(m) &= ~EXTF_READONLY;

			/* "Free" into the intermediate cache */
			if (m_free_func == NULL) {
				mcache_free(m_cache(MC_MBUF_CL), m);
			} else if (m_free_func == m_bigfree) {
				mcache_free(m_cache(MC_MBUF_BIGCL), m);
			} else {
				VERIFY(m_free_func == m_16kfree);
				mcache_free(m_cache(MC_MBUF_16KCL), m);
			}
			return n;
		}
	}

	mtype_stat_dec(m->m_type);
	mtype_stat_inc(MT_FREE);

	m->m_type = MT_FREE;
	m->m_flags = m->m_len = 0;
	m->m_next = m->m_nextpkt = NULL;

	mcache_free(m_cache(MC_MBUF), m);

	return n;
}

__private_extern__ struct mbuf *
m_clattach(struct mbuf *m, int type, caddr_t extbuf,
    void (*extfree)(caddr_t, u_int, caddr_t), size_t extsize, caddr_t extarg,
    int wait, int pair)
{
	struct ext_ref *rfa = NULL;

	/*
	 * If pairing is requested and an existing mbuf is provided, reject
	 * it if it's already been paired to another cluster.  Otherwise,
	 * allocate a new one or free any existing below.
	 */
	if ((m != NULL && MBUF_IS_PAIRED(m)) ||
	    (m == NULL && (m = _M_GETHDR(wait, type)) == NULL)) {
		return NULL;
	}

	if (m->m_flags & M_EXT) {
		/*
		 * Make sure that we don't touch any ext_ref
		 * member after we decrement the reference count
		 * since that may lead to use-after-free
		 * when we do not hold the last reference.
		 */
		const bool composite = !!(MEXT_FLAGS(m) & EXTF_COMPOSITE);
		VERIFY(!(MEXT_FLAGS(m) & EXTF_PAIRED) && MEXT_PMBUF(m) == NULL);
		const m_ext_free_func_t m_free_func = m_get_ext_free(m);
		const uint16_t minref = MEXT_MINREF(m);
		const uint16_t refcnt = m_decref(m);

		if (refcnt == minref && !composite) {
			if (m_free_func == NULL) {
				mcache_free(m_cache(MC_CL), m->m_ext.ext_buf);
			} else if (m_free_func == m_bigfree) {
				mcache_free(m_cache(MC_BIGCL),
				    m->m_ext.ext_buf);
			} else if (m_free_func == m_16kfree) {
				mcache_free(m_cache(MC_16KCL),
				    m->m_ext.ext_buf);
			} else {
				(*m_free_func)(m->m_ext.ext_buf,
				    m->m_ext.ext_size, m_get_ext_arg(m));
			}
			/* Re-use the reference structure */
			rfa = m_get_rfa(m);
		} else if (refcnt == minref && composite) {
			VERIFY(m->m_type != MT_FREE);

			mtype_stat_dec(m->m_type);
			mtype_stat_inc(MT_FREE);

			m->m_type = MT_FREE;
			m->m_flags = M_EXT;
			m->m_len = 0;
			m->m_next = m->m_nextpkt = NULL;

			/*
			 * MEXT_FLAGS is safe to access here
			 * since we are now sure that we held
			 * the last reference to ext_ref.
			 */
			MEXT_FLAGS(m) &= ~EXTF_READONLY;

			/* "Free" into the intermediate cache */
			if (m_free_func == NULL) {
				mcache_free(m_cache(MC_MBUF_CL), m);
			} else if (m_free_func == m_bigfree) {
				mcache_free(m_cache(MC_MBUF_BIGCL), m);
			} else {
				VERIFY(m_free_func == m_16kfree);
				mcache_free(m_cache(MC_MBUF_16KCL), m);
			}
			/*
			 * Allocate a new mbuf, since we didn't divorce
			 * the composite mbuf + cluster pair above.
			 */
			if ((m = _M_GETHDR(wait, type)) == NULL) {
				return NULL;
			}
		}
	}

	if (rfa == NULL &&
	    (rfa = mcache_alloc(ref_cache, MSLEEPF(wait))) == NULL) {
		m_free(m);
		return NULL;
	}

	if (!pair) {
		mext_init(m, extbuf, extsize, extfree, extarg, rfa,
		    0, 1, 0, 0, 0, NULL);
	} else {
		mext_init(m, extbuf, extsize, extfree, (caddr_t)m, rfa,
		    1, 1, 1, EXTF_PAIRED, 0, m);
	}

	return m;
}

/*
 * Perform `fast' allocation mbuf clusters from a cache of recently-freed
 * clusters. (If the cache is empty, new clusters are allocated en-masse.)
 */
struct mbuf *
m_getcl(int wait, int type, int flags)
{
	struct mbuf *m = NULL;
	int hdr = (flags & M_PKTHDR);

	int mcflags = MSLEEPF(wait);

	/* Is this due to a non-blocking retry?  If so, then try harder */
	if (mcflags & MCR_NOSLEEP) {
		mcflags |= MCR_TRYHARD;
	}

	m = mcache_alloc(m_cache(MC_MBUF_CL), mcflags);
	if (m != NULL) {
		u_int16_t flag;
		struct ext_ref *rfa;
		void *cl;

		VERIFY(m->m_type == MT_FREE && m->m_flags == M_EXT);
		cl = m->m_ext.ext_buf;
		rfa = m_get_rfa(m);

		ASSERT(cl != NULL && rfa != NULL);
		VERIFY(MBUF_IS_COMPOSITE(m) && m_get_ext_free(m) == NULL);

		flag = MEXT_FLAGS(m);

		mbuf_init(m, hdr, type);
		MBUF_CL_INIT(m, cl, rfa, 1, flag);

		mtype_stat_inc(type);
		mtype_stat_dec(MT_FREE);
	}
	return m;
}

/* m_mclget() add an mbuf cluster to a normal mbuf */
struct mbuf *
m_mclget(struct mbuf *m, int wait)
{
	struct ext_ref *rfa = NULL;

	if ((rfa = mcache_alloc(ref_cache, MSLEEPF(wait))) == NULL) {
		return m;
	}
	m->m_ext.ext_buf = m_mclalloc(wait);
	if (m->m_ext.ext_buf != NULL) {
		MBUF_CL_INIT(m, m->m_ext.ext_buf, rfa, 1, 0);
	} else {
		mcache_free(ref_cache, rfa);
	}

	return m;
}

/* Allocate an mbuf cluster */
caddr_t
m_mclalloc(int wait)
{
	int mcflags = MSLEEPF(wait);

	/* Is this due to a non-blocking retry?  If so, then try harder */
	if (mcflags & MCR_NOSLEEP) {
		mcflags |= MCR_TRYHARD;
	}

	return mcache_alloc(m_cache(MC_CL), mcflags);
}

/* Free an mbuf cluster */
void
m_mclfree(caddr_t p)
{
	mcache_free(m_cache(MC_CL), p);
}

__private_extern__ caddr_t
m_bigalloc(int wait)
{
	int mcflags = MSLEEPF(wait);

	/* Is this due to a non-blocking retry?  If so, then try harder */
	if (mcflags & MCR_NOSLEEP) {
		mcflags |= MCR_TRYHARD;
	}

	return mcache_alloc(m_cache(MC_BIGCL), mcflags);
}

__private_extern__ void
m_bigfree(caddr_t p, __unused u_int size, __unused caddr_t arg)
{
	mcache_free(m_cache(MC_BIGCL), p);
}

/* m_mbigget() add an 4KB mbuf cluster to a normal mbuf */
__private_extern__ struct mbuf *
m_mbigget(struct mbuf *m, int wait)
{
	struct ext_ref *rfa = NULL;

	if ((rfa = mcache_alloc(ref_cache, MSLEEPF(wait))) == NULL) {
		return m;
	}
	m->m_ext.ext_buf = m_bigalloc(wait);
	if (m->m_ext.ext_buf != NULL) {
		MBUF_BIGCL_INIT(m, m->m_ext.ext_buf, rfa, 1, 0);
	} else {
		mcache_free(ref_cache, rfa);
	}
	return m;
}

__private_extern__ caddr_t
m_16kalloc(int wait)
{
	int mcflags = MSLEEPF(wait);

	/* Is this due to a non-blocking retry?  If so, then try harder */
	if (mcflags & MCR_NOSLEEP) {
		mcflags |= MCR_TRYHARD;
	}

	return mcache_alloc(m_cache(MC_16KCL), mcflags);
}

__private_extern__ void
m_16kfree(caddr_t p, __unused u_int size, __unused caddr_t arg)
{
	mcache_free(m_cache(MC_16KCL), p);
}

/* m_m16kget() add a 16KB mbuf cluster to a normal mbuf */
__private_extern__ struct mbuf *
m_m16kget(struct mbuf *m, int wait)
{
	struct ext_ref *rfa = NULL;

	if ((rfa = mcache_alloc(ref_cache, MSLEEPF(wait))) == NULL) {
		return m;
	}
	m->m_ext.ext_buf =  m_16kalloc(wait);
	if (m->m_ext.ext_buf != NULL) {
		MBUF_16KCL_INIT(m, m->m_ext.ext_buf, rfa, 1, 0);
	} else {
		mcache_free(ref_cache, rfa);
	}

	return m;
}

/*
 * Return a list of mbuf hdrs that point to clusters.  Try for num_needed;
 * if wantall is not set, return whatever number were available.  Set up the
 * first num_with_pkthdrs with mbuf hdrs configured as packet headers; these
 * are chained on the m_nextpkt field.  Any packets requested beyond this
 * are chained onto the last packet header's m_next field.  The size of
 * the cluster is controlled by the parameter bufsize.
 */
__private_extern__ struct mbuf *
m_getpackets_internal(unsigned int *num_needed, int num_with_pkthdrs,
    int wait, int wantall, size_t bufsize)
{
	struct mbuf *m = NULL;
	struct mbuf **np, *top;
	unsigned int pnum, needed = *num_needed;
	mcache_obj_t *mp_list = NULL;
	int mcflags = MSLEEPF(wait);
	mcache_t *cp;
	u_int16_t flag;
	struct ext_ref *rfa;
	void *cl;

	ASSERT(bufsize == m_maxsize(MC_CL) ||
	    bufsize == m_maxsize(MC_BIGCL) ||
	    bufsize == m_maxsize(MC_16KCL));

	top = NULL;
	np = &top;
	pnum = 0;

	/*
	 * The caller doesn't want all the requested buffers; only some.
	 * Try hard to get what we can, but don't block.  This effectively
	 * overrides MCR_SLEEP, since this thread will not go to sleep
	 * if we can't get all the buffers.
	 */
	if (!wantall || (mcflags & MCR_NOSLEEP)) {
		mcflags |= MCR_TRYHARD;
	}

	/* Allocate the composite mbuf + cluster elements from the cache */
	if (bufsize == m_maxsize(MC_CL)) {
		cp = m_cache(MC_MBUF_CL);
	} else if (bufsize == m_maxsize(MC_BIGCL)) {
		cp = m_cache(MC_MBUF_BIGCL);
	} else {
		cp = m_cache(MC_MBUF_16KCL);
	}
	needed = mcache_alloc_ext(cp, &mp_list, needed, mcflags);

	for (pnum = 0; pnum < needed; pnum++) {
		m = (struct mbuf *)mp_list;
		mp_list = mp_list->obj_next;

		VERIFY(m->m_type == MT_FREE && m->m_flags == M_EXT);
		cl = m->m_ext.ext_buf;
		rfa = m_get_rfa(m);

		ASSERT(cl != NULL && rfa != NULL);
		VERIFY(MBUF_IS_COMPOSITE(m));

		flag = MEXT_FLAGS(m);

		mbuf_init(m, num_with_pkthdrs, MT_DATA);
		if (bufsize == m_maxsize(MC_16KCL)) {
			MBUF_16KCL_INIT(m, cl, rfa, 1, flag);
		} else if (bufsize == m_maxsize(MC_BIGCL)) {
			MBUF_BIGCL_INIT(m, cl, rfa, 1, flag);
		} else {
			MBUF_CL_INIT(m, cl, rfa, 1, flag);
		}

		if (num_with_pkthdrs > 0) {
			--num_with_pkthdrs;
		}

		*np = m;
		if (num_with_pkthdrs > 0) {
			np = &m->m_nextpkt;
		} else {
			np = &m->m_next;
		}
	}
	ASSERT(pnum != *num_needed || mp_list == NULL);
	if (mp_list != NULL) {
		mcache_free_ext(cp, mp_list);
	}
	if (pnum > 0) {
		mtype_stat_add(MT_DATA, pnum);
		mtype_stat_sub(MT_FREE, pnum);
	}

	if (wantall && (pnum != *num_needed)) {
		if (top != NULL) {
			m_freem_list(top);
		}
		return NULL;
	}

	if (pnum > *num_needed) {
		printf("%s: File a radar related to <rdar://10146739>. \
			needed = %u, pnum = %u, num_needed = %u \n",
		    __func__, needed, pnum, *num_needed);
	}
	*num_needed = pnum;

	return top;
}

/*
 * Return list of mbuf linked by m_nextpkt.  Try for numlist, and if
 * wantall is not set, return whatever number were available.  The size of
 * each mbuf in the list is controlled by the parameter packetlen.  Each
 * mbuf of the list may have a chain of mbufs linked by m_next.  Each mbuf
 * in the chain is called a segment.  If maxsegments is not null and the
 * value pointed to is not null, this specify the maximum number of segments
 * for a chain of mbufs.  If maxsegments is zero or the value pointed to
 * is zero the caller does not have any restriction on the number of segments.
 * The actual  number of segments of a mbuf chain is return in the value
 * pointed to by maxsegments.
 */
__private_extern__ struct mbuf *
m_allocpacket_internal(unsigned int *numlist, size_t packetlen,
    unsigned int *maxsegments, int wait, int wantall, size_t wantsize)
{
	struct mbuf **np, *top, *first = NULL;
	size_t bufsize, r_bufsize;
	unsigned int num = 0;
	unsigned int nsegs = 0;
	unsigned int needed = 0, resid;
	int mcflags = MSLEEPF(wait);
	mcache_obj_t *mp_list = NULL, *rmp_list = NULL;
	mcache_t *cp = NULL, *rcp = NULL;

	if (*numlist == 0) {
		os_log(OS_LOG_DEFAULT, "m_allocpacket_internal *numlist is 0");
		return NULL;
	}

	top = NULL;
	np = &top;

	if (wantsize == 0) {
		if (packetlen <= MINCLSIZE) {
			bufsize = packetlen;
		} else if (packetlen > m_maxsize(MC_CL)) {
			/* Use 4KB if jumbo cluster pool isn't available */
			if (packetlen <= m_maxsize(MC_BIGCL)) {
				bufsize = m_maxsize(MC_BIGCL);
			} else {
				bufsize = m_maxsize(MC_16KCL);
			}
		} else {
			bufsize = m_maxsize(MC_CL);
		}
	} else if (wantsize == m_maxsize(MC_CL) ||
	    wantsize == m_maxsize(MC_BIGCL) ||
	    wantsize == m_maxsize(MC_16KCL)) {
		bufsize = wantsize;
	} else {
		*numlist = 0;
		os_log(OS_LOG_DEFAULT, "m_allocpacket_internal wantsize unsupported");
		return NULL;
	}

	if (bufsize <= MHLEN) {
		nsegs = 1;
	} else if (bufsize <= MINCLSIZE) {
		if (maxsegments != NULL && *maxsegments == 1) {
			bufsize = m_maxsize(MC_CL);
			nsegs = 1;
		} else {
			nsegs = 2;
		}
	} else if (bufsize == m_maxsize(MC_16KCL)) {
		nsegs = ((packetlen - 1) >> M16KCLSHIFT) + 1;
	} else if (bufsize == m_maxsize(MC_BIGCL)) {
		nsegs = ((packetlen - 1) >> MBIGCLSHIFT) + 1;
	} else {
		nsegs = ((packetlen - 1) >> MCLSHIFT) + 1;
	}
	if (maxsegments != NULL) {
		if (*maxsegments && nsegs > *maxsegments) {
			*maxsegments = nsegs;
			*numlist = 0;
			os_log(OS_LOG_DEFAULT, "m_allocpacket_internal nsegs > *maxsegments");
			return NULL;
		}
		*maxsegments = nsegs;
	}

	/*
	 * The caller doesn't want all the requested buffers; only some.
	 * Try hard to get what we can, but don't block.  This effectively
	 * overrides MCR_SLEEP, since this thread will not go to sleep
	 * if we can't get all the buffers.
	 */
	if (!wantall || (mcflags & MCR_NOSLEEP)) {
		mcflags |= MCR_TRYHARD;
	}

	/*
	 * Simple case where all elements in the lists/chains are mbufs.
	 * Unless bufsize is greater than MHLEN, each segment chain is made
	 * up of exactly 1 mbuf.  Otherwise, each segment chain is made up
	 * of 2 mbufs; the second one is used for the residual data, i.e.
	 * the remaining data that cannot fit into the first mbuf.
	 */
	if (bufsize <= MINCLSIZE) {
		/* Allocate the elements in one shot from the mbuf cache */
		ASSERT(bufsize <= MHLEN || nsegs == 2);
		cp = m_cache(MC_MBUF);
		needed = mcache_alloc_ext(cp, &mp_list,
		    (*numlist) * nsegs, mcflags);

		/*
		 * The number of elements must be even if we are to use an
		 * mbuf (instead of a cluster) to store the residual data.
		 * If we couldn't allocate the requested number of mbufs,
		 * trim the number down (if it's odd) in order to avoid
		 * creating a partial segment chain.
		 */
		if (bufsize > MHLEN && (needed & 0x1)) {
			needed--;
		}

		while (num < needed) {
			struct mbuf *m = NULL;

			m = (struct mbuf *)mp_list;
			mp_list = mp_list->obj_next;
			ASSERT(m != NULL);

			mbuf_init(m, 1, MT_DATA);
			num++;
			if (bufsize > MHLEN) {
				/* A second mbuf for this segment chain */
				m->m_next = (struct mbuf *)mp_list;
				mp_list = mp_list->obj_next;

				ASSERT(m->m_next != NULL);

				mbuf_init(m->m_next, 0, MT_DATA);
				num++;
			}
			*np = m;
			np = &m->m_nextpkt;
		}
		ASSERT(num != *numlist || mp_list == NULL);

		if (num > 0) {
			mtype_stat_add(MT_DATA, num);
			mtype_stat_sub(MT_FREE, num);
		}
		num /= nsegs;

		/* We've got them all; return to caller */
		if (num == *numlist) {
			return top;
		}

		goto fail;
	}

	/*
	 * Complex cases where elements are made up of one or more composite
	 * mbufs + cluster, depending on packetlen.  Each N-segment chain can
	 * be illustrated as follows:
	 *
	 * [mbuf + cluster 1] [mbuf + cluster 2] ... [mbuf + cluster N]
	 *
	 * Every composite mbuf + cluster element comes from the intermediate
	 * cache (either MC_MBUF_CL or MC_MBUF_BIGCL).  For space efficiency,
	 * the last composite element will come from the MC_MBUF_CL cache,
	 * unless the residual data is larger than 2KB where we use the
	 * big cluster composite cache (MC_MBUF_BIGCL) instead.  Residual
	 * data is defined as extra data beyond the first element that cannot
	 * fit into the previous element, i.e. there is no residual data if
	 * the chain only has 1 segment.
	 */
	r_bufsize = bufsize;
	resid = packetlen > bufsize ? packetlen % bufsize : 0;
	if (resid > 0) {
		/* There is residual data; figure out the cluster size */
		if (wantsize == 0 && packetlen > MINCLSIZE) {
			/*
			 * Caller didn't request that all of the segments
			 * in the chain use the same cluster size; use the
			 * smaller of the cluster sizes.
			 */
			if (resid > m_maxsize(MC_BIGCL)) {
				r_bufsize = m_maxsize(MC_16KCL);
			} else if (resid > m_maxsize(MC_CL)) {
				r_bufsize = m_maxsize(MC_BIGCL);
			} else {
				r_bufsize = m_maxsize(MC_CL);
			}
		} else {
			/* Use the same cluster size as the other segments */
			resid = 0;
		}
	}

	needed = *numlist;
	if (resid > 0) {
		/*
		 * Attempt to allocate composite mbuf + cluster elements for
		 * the residual data in each chain; record the number of such
		 * elements that can be allocated so that we know how many
		 * segment chains we can afford to create.
		 */
		if (r_bufsize <= m_maxsize(MC_CL)) {
			rcp = m_cache(MC_MBUF_CL);
		} else if (r_bufsize <= m_maxsize(MC_BIGCL)) {
			rcp = m_cache(MC_MBUF_BIGCL);
		} else {
			rcp = m_cache(MC_MBUF_16KCL);
		}
		needed = mcache_alloc_ext(rcp, &rmp_list, *numlist, mcflags);
		if (needed == 0) {
			goto fail;
		}

		/* This is temporarily reduced for calculation */
		ASSERT(nsegs > 1);
		nsegs--;
	}

	/*
	 * Attempt to allocate the rest of the composite mbuf + cluster
	 * elements for the number of segment chains that we need.
	 */
	if (bufsize <= m_maxsize(MC_CL)) {
		cp = m_cache(MC_MBUF_CL);
	} else if (bufsize <= m_maxsize(MC_BIGCL)) {
		cp = m_cache(MC_MBUF_BIGCL);
	} else {
		cp = m_cache(MC_MBUF_16KCL);
	}
	needed = mcache_alloc_ext(cp, &mp_list, needed * nsegs, mcflags);

	/* Round it down to avoid creating a partial segment chain */
	needed = (needed / nsegs) * nsegs;
	if (needed == 0) {
		goto fail;
	}

	if (resid > 0) {
		/*
		 * We're about to construct the chain(s); take into account
		 * the number of segments we have created above to hold the
		 * residual data for each chain, as well as restore the
		 * original count of segments per chain.
		 */
		ASSERT(nsegs > 0);
		needed += needed / nsegs;
		nsegs++;
	}

	for (;;) {
		struct mbuf *m = NULL;
		u_int16_t flag;
		struct ext_ref *rfa;
		void *cl;
		int pkthdr;
		m_ext_free_func_t m_free_func;

		++num;

		if (nsegs == 1 || (num % nsegs) != 0 || resid == 0) {
			m = (struct mbuf *)mp_list;
			mp_list = mp_list->obj_next;
		} else {
			m = (struct mbuf *)rmp_list;
			rmp_list = rmp_list->obj_next;
		}
		m_free_func = m_get_ext_free(m);
		ASSERT(m != NULL);
		VERIFY(m->m_type == MT_FREE && m->m_flags == M_EXT);
		VERIFY(m_free_func == NULL || m_free_func == m_bigfree ||
		    m_free_func == m_16kfree);

		cl = m->m_ext.ext_buf;
		rfa = m_get_rfa(m);

		ASSERT(cl != NULL && rfa != NULL);
		VERIFY(MBUF_IS_COMPOSITE(m));

		flag = MEXT_FLAGS(m);

		pkthdr = (nsegs == 1 || (num % nsegs) == 1);
		if (pkthdr) {
			first = m;
		}
		mbuf_init(m, pkthdr, MT_DATA);
		if (m_free_func == m_16kfree) {
			MBUF_16KCL_INIT(m, cl, rfa, 1, flag);
		} else if (m_free_func == m_bigfree) {
			MBUF_BIGCL_INIT(m, cl, rfa, 1, flag);
		} else {
			MBUF_CL_INIT(m, cl, rfa, 1, flag);
		}

		*np = m;
		if ((num % nsegs) == 0) {
			np = &first->m_nextpkt;
		} else {
			np = &m->m_next;
		}

		if (num == needed) {
			break;
		}
	}

	if (num > 0) {
		mtype_stat_add(MT_DATA, num);
		mtype_stat_sub(MT_FREE, num);
	}

	num /= nsegs;

	/* We've got them all; return to caller */
	if (num == *numlist) {
		ASSERT(mp_list == NULL && rmp_list == NULL);
		return top;
	}

fail:
	/* Free up what's left of the above */
	if (mp_list != NULL) {
		mcache_free_ext(cp, mp_list);
	}
	if (rmp_list != NULL) {
		mcache_free_ext(rcp, rmp_list);
	}
	if (wantall && top != NULL) {
		m_freem_list(top);
		*numlist = 0;
		return NULL;
	}
	*numlist = num;
	return top;
}

/*
 * Free an mbuf list (m_nextpkt) while following m_next.  Returns the count
 * for mbufs packets freed.  Used by the drivers.
 */
int
m_freem_list(struct mbuf *m)
{
	struct mbuf *nextpkt;
	mcache_obj_t *mp_list = NULL;
	mcache_obj_t *mcl_list = NULL;
	mcache_obj_t *mbc_list = NULL;
	mcache_obj_t *m16k_list = NULL;
	mcache_obj_t *m_mcl_list = NULL;
	mcache_obj_t *m_mbc_list = NULL;
	mcache_obj_t *m_m16k_list = NULL;
	mcache_obj_t *ref_list = NULL;
	int pktcount = 0;
	int mt_free = 0, mt_data = 0, mt_header = 0, mt_soname = 0, mt_tag = 0;

	while (m != NULL) {
		pktcount++;

		nextpkt = m->m_nextpkt;
		m->m_nextpkt = NULL;

		while (m != NULL) {
			struct mbuf *next = m->m_next;
			mcache_obj_t *o, *rfa;
			if (m->m_type == MT_FREE) {
				panic("m_free: freeing an already freed mbuf");
			}

			if (m->m_flags & M_PKTHDR) {
				/* Free the aux data and tags if there is any */
				m_tag_delete_chain(m);
				m_do_tx_compl_callback(m, NULL);
			}

			if (!(m->m_flags & M_EXT)) {
				mt_free++;
				goto simple_free;
			}

			if (MBUF_IS_PAIRED(m) && m_free_paired(m)) {
				m = next;
				continue;
			}

			mt_free++;

			o = (mcache_obj_t *)(void *)m->m_ext.ext_buf;
			/*
			 * Make sure that we don't touch any ext_ref
			 * member after we decrement the reference count
			 * since that may lead to use-after-free
			 * when we do not hold the last reference.
			 */
			const bool composite = !!(MEXT_FLAGS(m) & EXTF_COMPOSITE);
			const m_ext_free_func_t m_free_func = m_get_ext_free(m);
			const uint16_t minref = MEXT_MINREF(m);
			const uint16_t refcnt = m_decref(m);
			if (refcnt == minref && !composite) {
				if (m_free_func == NULL) {
					o->obj_next = mcl_list;
					mcl_list = o;
				} else if (m_free_func == m_bigfree) {
					o->obj_next = mbc_list;
					mbc_list = o;
				} else if (m_free_func == m_16kfree) {
					o->obj_next = m16k_list;
					m16k_list = o;
				} else {
					(*(m_free_func))((caddr_t)o,
					    m->m_ext.ext_size,
					    m_get_ext_arg(m));
				}
				rfa = (mcache_obj_t *)(void *)m_get_rfa(m);
				rfa->obj_next = ref_list;
				ref_list = rfa;
				m_set_ext(m, NULL, NULL, NULL);
			} else if (refcnt == minref && composite) {
				VERIFY(!(MEXT_FLAGS(m) & EXTF_PAIRED));
				/*
				 * Amortize the costs of atomic operations
				 * by doing them at the end, if possible.
				 */
				if (m->m_type == MT_DATA) {
					mt_data++;
				} else if (m->m_type == MT_HEADER) {
					mt_header++;
				} else if (m->m_type == MT_SONAME) {
					mt_soname++;
				} else if (m->m_type == MT_TAG) {
					mt_tag++;
				} else {
					mtype_stat_dec(m->m_type);
				}

				m->m_type = MT_FREE;
				m->m_flags = M_EXT;
				m->m_len = 0;
				m->m_next = m->m_nextpkt = NULL;

				/*
				 * MEXT_FLAGS is safe to access here
				 * since we are now sure that we held
				 * the last reference to ext_ref.
				 */
				MEXT_FLAGS(m) &= ~EXTF_READONLY;

				/* "Free" into the intermediate cache */
				o = (mcache_obj_t *)m;
				if (m_free_func == NULL) {
					o->obj_next = m_mcl_list;
					m_mcl_list = o;
				} else if (m_free_func == m_bigfree) {
					o->obj_next = m_mbc_list;
					m_mbc_list = o;
				} else {
					VERIFY(m_free_func == m_16kfree);
					o->obj_next = m_m16k_list;
					m_m16k_list = o;
				}
				m = next;
				continue;
			}
simple_free:
			/*
			 * Amortize the costs of atomic operations
			 * by doing them at the end, if possible.
			 */
			if (m->m_type == MT_DATA) {
				mt_data++;
			} else if (m->m_type == MT_HEADER) {
				mt_header++;
			} else if (m->m_type == MT_SONAME) {
				mt_soname++;
			} else if (m->m_type == MT_TAG) {
				mt_tag++;
			} else if (m->m_type != MT_FREE) {
				mtype_stat_dec(m->m_type);
			}

			m->m_type = MT_FREE;
			m->m_flags = m->m_len = 0;
			m->m_next = m->m_nextpkt = NULL;

			((mcache_obj_t *)m)->obj_next = mp_list;
			mp_list = (mcache_obj_t *)m;

			m = next;
		}

		m = nextpkt;
	}

	if (mt_free > 0) {
		mtype_stat_add(MT_FREE, mt_free);
	}
	if (mt_data > 0) {
		mtype_stat_sub(MT_DATA, mt_data);
	}
	if (mt_header > 0) {
		mtype_stat_sub(MT_HEADER, mt_header);
	}
	if (mt_soname > 0) {
		mtype_stat_sub(MT_SONAME, mt_soname);
	}
	if (mt_tag > 0) {
		mtype_stat_sub(MT_TAG, mt_tag);
	}
	if (mp_list != NULL) {
		mcache_free_ext(m_cache(MC_MBUF), mp_list);
	}
	if (mcl_list != NULL) {
		mcache_free_ext(m_cache(MC_CL), mcl_list);
	}
	if (mbc_list != NULL) {
		mcache_free_ext(m_cache(MC_BIGCL), mbc_list);
	}
	if (m16k_list != NULL) {
		mcache_free_ext(m_cache(MC_16KCL), m16k_list);
	}
	if (m_mcl_list != NULL) {
		mcache_free_ext(m_cache(MC_MBUF_CL), m_mcl_list);
	}
	if (m_mbc_list != NULL) {
		mcache_free_ext(m_cache(MC_MBUF_BIGCL), m_mbc_list);
	}
	if (m_m16k_list != NULL) {
		mcache_free_ext(m_cache(MC_MBUF_16KCL), m_m16k_list);
	}
	if (ref_list != NULL) {
		mcache_free_ext(ref_cache, ref_list);
	}

	return pktcount;
}

/*
 * Equivalent to m_copym except that all necessary mbuf hdrs are allocated
 * within this routine also.
 *
 * The last mbuf and offset accessed are passed in and adjusted on return to
 * avoid having to iterate over the entire mbuf chain each time.
 */
struct mbuf *
m_copym_with_hdrs(struct mbuf *m0, int off0, int len0, int wait,
    struct mbuf **m_lastm, int *m_off, uint32_t mode)
{
	struct mbuf *m = m0, *n, **np = NULL;
	int off = off0, len = len0;
	struct mbuf *top = NULL;
	int mcflags = MSLEEPF(wait);
	mcache_obj_t *list = NULL;
	int copyhdr = 0;
	int type = 0;
	int needed = 0;

	if (off == 0 && (m->m_flags & M_PKTHDR)) {
		copyhdr = 1;
	}

	if (m_lastm != NULL && *m_lastm != NULL) {
		if (off0 >= *m_off) {
			m = *m_lastm;
			off = off0 - *m_off;
		}
	}

	while (off >= m->m_len) {
		off -= m->m_len;
		m = m->m_next;
	}

	n = m;
	while (len > 0) {
		needed++;
		len -= MIN(len, (n->m_len - ((needed == 1) ? off : 0)));
		n = n->m_next;
	}
	needed++;
	len = len0;

	/*
	 * If the caller doesn't want to be put to sleep, mark it with
	 * MCR_TRYHARD so that we may reclaim buffers from other places
	 * before giving up.
	 */
	if (mcflags & MCR_NOSLEEP) {
		mcflags |= MCR_TRYHARD;
	}

	if (mcache_alloc_ext(m_cache(MC_MBUF), &list, needed,
	    mcflags) != needed) {
		goto nospace;
	}

	needed = 0;
	while (len > 0) {
		n = (struct mbuf *)list;
		list = list->obj_next;
		ASSERT(n != NULL && m != NULL);

		type = (top == NULL) ? MT_HEADER : m->m_type;
		mbuf_init(n, (top == NULL), type);

		if (top == NULL) {
			top = n;
			np = &top->m_next;
			continue;
		} else {
			needed++;
			*np = n;
		}

		if (copyhdr) {
			if ((mode == M_COPYM_MOVE_HDR) ||
			    (mode == M_COPYM_MUST_MOVE_HDR)) {
				M_COPY_PKTHDR(n, m);
			} else if ((mode == M_COPYM_COPY_HDR) ||
			    (mode == M_COPYM_MUST_COPY_HDR)) {
				if (m_dup_pkthdr(n, m, wait) == 0) {
					goto nospace;
				}
			}
			n->m_pkthdr.len = len;
			copyhdr = 0;
		}
		n->m_len = MIN(len, (m->m_len - off));

		if (m->m_flags & M_EXT) {
			n->m_ext = m->m_ext;
			m_incref(m);
			n->m_data = m->m_data + off;
			n->m_flags |= M_EXT;
		} else {
			if (m_mtod_end(n) > m_mtod_upper_bound(n)) {
				panic("%s n %p copy overflow",
				    __func__, n);
			}

			bcopy(mtod(m, caddr_t) + off, mtod(n, caddr_t),
			    (unsigned)n->m_len);
		}
		len -= n->m_len;

		if (len == 0) {
			if (m_lastm != NULL) {
				*m_lastm = m;
				*m_off = off0 + len0 - (off + n->m_len);
			}
			break;
		}
		off = 0;
		m = m->m_next;
		np = &n->m_next;
	}

	mtype_stat_inc(MT_HEADER);
	mtype_stat_add(type, needed);
	mtype_stat_sub(MT_FREE, needed + 1);

	ASSERT(list == NULL);

	return top;

nospace:
	if (list != NULL) {
		mcache_free_ext(m_cache(MC_MBUF), list);
	}
	if (top != NULL) {
		m_freem(top);
	}
	return NULL;
}

#ifndef MBUF_GROWTH_NORMAL_THRESH
#define MBUF_GROWTH_NORMAL_THRESH 25
#endif

/*
 * Cluster freelist allocation check.
 */
static int
m_howmany(int num, size_t bufsize)
{
	int i = 0, j = 0;
	u_int32_t m_mbclusters, m_clusters, m_bigclusters, m_16kclusters;
	u_int32_t m_mbfree, m_clfree, m_bigclfree, m_16kclfree;
	u_int32_t sumclusters, freeclusters;
	u_int32_t percent_pool, percent_kmem;
	u_int32_t mb_growth, mb_growth_thresh;

	VERIFY(bufsize == m_maxsize(MC_BIGCL) ||
	    bufsize == m_maxsize(MC_16KCL));

	LCK_MTX_ASSERT(mbuf_mlock, LCK_MTX_ASSERT_OWNED);

	/* Numbers in 2K cluster units */
	m_mbclusters = m_total(MC_MBUF) >> NMBPCLSHIFT;
	m_clusters = m_total(MC_CL);
	m_bigclusters = m_total(MC_BIGCL) << NCLPBGSHIFT;
	m_16kclusters = m_total(MC_16KCL);
	sumclusters = m_mbclusters + m_clusters + m_bigclusters;

	m_mbfree = m_infree(MC_MBUF) >> NMBPCLSHIFT;
	m_clfree = m_infree(MC_CL);
	m_bigclfree = m_infree(MC_BIGCL) << NCLPBGSHIFT;
	m_16kclfree = m_infree(MC_16KCL);
	freeclusters = m_mbfree + m_clfree + m_bigclfree;

	/* Bail if we've maxed out the mbuf memory map */
	if ((bufsize == m_maxsize(MC_BIGCL) && sumclusters >= nclusters) ||
	    (bufsize == m_maxsize(MC_16KCL) &&
	    (m_16kclusters << NCLPJCLSHIFT) >= njcl)) {
		mbwdog_logger("maxed out nclusters (%u >= %u) or njcl (%u >= %u)",
		    sumclusters, nclusters,
		    (m_16kclusters << NCLPJCLSHIFT), njcl);
		return 0;
	}

	if (bufsize == m_maxsize(MC_BIGCL)) {
		/* Under minimum */
		if (m_bigclusters < m_minlimit(MC_BIGCL)) {
			return m_minlimit(MC_BIGCL) - m_bigclusters;
		}

		percent_pool =
		    ((sumclusters - freeclusters) * 100) / sumclusters;
		percent_kmem = (sumclusters * 100) / nclusters;

		/*
		 * If a light/normal user, grow conservatively (75%)
		 * If a heavy user, grow aggressively (50%)
		 */
		if (percent_kmem < MBUF_GROWTH_NORMAL_THRESH) {
			mb_growth = MB_GROWTH_NORMAL;
		} else {
			mb_growth = MB_GROWTH_AGGRESSIVE;
		}

		if (percent_kmem < 5) {
			/* For initial allocations */
			i = num;
		} else {
			/* Return if >= MBIGCL_LOWAT clusters available */
			if (m_infree(MC_BIGCL) >= MBIGCL_LOWAT &&
			    m_total(MC_BIGCL) >=
			    MBIGCL_LOWAT + m_minlimit(MC_BIGCL)) {
				return 0;
			}

			/* Ensure at least num clusters are accessible */
			if (num >= m_infree(MC_BIGCL)) {
				i = num - m_infree(MC_BIGCL);
			}
			if (num > m_total(MC_BIGCL) - m_minlimit(MC_BIGCL)) {
				j = num - (m_total(MC_BIGCL) -
				    m_minlimit(MC_BIGCL));
			}

			i = MAX(i, j);

			/*
			 * Grow pool if percent_pool > 75 (normal growth)
			 * or percent_pool > 50 (aggressive growth).
			 */
			mb_growth_thresh = 100 - (100 / (1 << mb_growth));
			if (percent_pool > mb_growth_thresh) {
				j = ((sumclusters + num) >> mb_growth) -
				    freeclusters;
			}
			i = MAX(i, j);
		}

		/* Check to ensure we didn't go over limits */
		if (i + m_bigclusters >= m_maxlimit(MC_BIGCL)) {
			i = m_maxlimit(MC_BIGCL) - m_bigclusters;
		}
		if ((i << 1) + sumclusters >= nclusters) {
			i = (nclusters - sumclusters) >> 1;
		}
		VERIFY((m_total(MC_BIGCL) + i) <= m_maxlimit(MC_BIGCL));
		VERIFY(sumclusters + (i << 1) <= nclusters);
	} else { /* 16K CL */
		/* Ensure at least num clusters are available */
		if (num >= m_16kclfree) {
			i = num - m_16kclfree;
		}

		/* Always grow 16KCL pool aggressively */
		if (((m_16kclusters + num) >> 1) > m_16kclfree) {
			j = ((m_16kclusters + num) >> 1) - m_16kclfree;
		}
		i = MAX(i, j);

		/* Check to ensure we don't go over limit */
		if ((i + m_total(MC_16KCL)) >= m_maxlimit(MC_16KCL)) {
			i = m_maxlimit(MC_16KCL) - m_total(MC_16KCL);
		}
	}
	return i;
}

uint64_t
mcl_to_paddr(char *addr)
{
	vm_offset_t base_phys;

	if (!MBUF_IN_MAP(addr)) {
		return 0;
	}
	base_phys = mcl_paddr[atop_64(addr - (char *)mbutl)];

	if (base_phys == 0) {
		return 0;
	}
	return (uint64_t)(ptoa_64(base_phys) | ((uint64_t)addr & PAGE_MASK));
}

/*
 * Inform the corresponding mcache(s) that there's a waiter below.
 */
static void
mbuf_waiter_inc(mbuf_class_t class, boolean_t comp)
{
	mcache_waiter_inc(m_cache(class));
	if (comp) {
		if (class == MC_CL) {
			mcache_waiter_inc(m_cache(MC_MBUF_CL));
		} else if (class == MC_BIGCL) {
			mcache_waiter_inc(m_cache(MC_MBUF_BIGCL));
		} else if (class == MC_16KCL) {
			mcache_waiter_inc(m_cache(MC_MBUF_16KCL));
		} else {
			mcache_waiter_inc(m_cache(MC_MBUF_CL));
			mcache_waiter_inc(m_cache(MC_MBUF_BIGCL));
		}
	}
}

/*
 * Inform the corresponding mcache(s) that there's no more waiter below.
 */
static void
mbuf_waiter_dec(mbuf_class_t class, boolean_t comp)
{
	mcache_waiter_dec(m_cache(class));
	if (comp) {
		if (class == MC_CL) {
			mcache_waiter_dec(m_cache(MC_MBUF_CL));
		} else if (class == MC_BIGCL) {
			mcache_waiter_dec(m_cache(MC_MBUF_BIGCL));
		} else if (class == MC_16KCL) {
			mcache_waiter_dec(m_cache(MC_MBUF_16KCL));
		} else {
			mcache_waiter_dec(m_cache(MC_MBUF_CL));
			mcache_waiter_dec(m_cache(MC_MBUF_BIGCL));
		}
	}
}

static bool mbuf_watchdog_defunct_active = false;

struct mbuf_watchdog_defunct_args {
	struct proc *top_app;
	uint32_t top_app_space_used;
	bool non_blocking;
};

extern const char *proc_name_address(void *p);

static void
mbuf_watchdog_defunct(thread_call_param_t arg0, thread_call_param_t arg1)
{
#pragma unused(arg0, arg1)
	struct mbuf_watchdog_defunct_args args = {};
	struct fileproc *fp = NULL;

	args.non_blocking = false;
	proc_iterate(PROC_ALLPROCLIST,
	    mbuf_watchdog_defunct_iterate, &args, NULL, NULL);

	/*
	 * Defunct all sockets from this app.
	 */
	if (args.top_app != NULL) {
		/* Restart the watchdog count. */
		lck_mtx_lock(mbuf_mlock);
		microuptime(&mb_wdtstart);
		lck_mtx_unlock(mbuf_mlock);
		os_log(OS_LOG_DEFAULT, "%s: defuncting all sockets from %s.%d",
		    __func__,
		    proc_name_address(args.top_app),
		    proc_pid(args.top_app));
		proc_fdlock(args.top_app);
		fdt_foreach(fp, args.top_app) {
			struct fileglob *fg = fp->fp_glob;
			struct socket *so = NULL;

			if (FILEGLOB_DTYPE(fg) != DTYPE_SOCKET) {
				continue;
			}
			so = (struct socket *)fp_get_data(fp);
			if (!socket_try_lock(so)) {
				continue;
			}
			if (sosetdefunct(args.top_app, so,
			    SHUTDOWN_SOCKET_LEVEL_DISCONNECT_ALL,
			    TRUE) == 0) {
				sodefunct(args.top_app, so,
				    SHUTDOWN_SOCKET_LEVEL_DISCONNECT_ALL);
			}
			socket_unlock(so, 0);
		}
		proc_fdunlock(args.top_app);
		proc_rele(args.top_app);
		mbstat.m_forcedefunct++;
	}
	mbuf_watchdog_defunct_active = false;
}

/*
 * Called during slab (blocking and non-blocking) allocation.  If there
 * is at least one waiter, and the time since the first waiter is blocked
 * is greater than the watchdog timeout, panic the system.
 */
static void
mbuf_watchdog(void)
{
	struct timeval now;
	unsigned int since;
	static thread_call_t defunct_tcall = NULL;

	if (mb_waiters == 0 || !mb_watchdog) {
		return;
	}

	LCK_MTX_ASSERT(mbuf_mlock, LCK_MTX_ASSERT_OWNED);

	microuptime(&now);
	since = now.tv_sec - mb_wdtstart.tv_sec;

	if (mbuf_watchdog_defunct_active) {
		/*
		 * Don't panic the system while we are trying
		 * to find sockets to defunct.
		 */
		return;
	}
	if (since >= MB_WDT_MAXTIME) {
		panic_plain("%s: %d waiters stuck for %u secs\n%s", __func__,
		    mb_waiters, since, mbuf_dump());
		/* NOTREACHED */
	}
	/*
	 * Check if we are about to panic the system due
	 * to lack of mbufs and start defuncting sockets
	 * from processes that use too many sockets.
	 *
	 * We're always called with the mbuf_mlock held,
	 * so that also protects mbuf_watchdog_defunct_active.
	 */
	if (since >= MB_WDT_MAXTIME / 2) {
		/*
		 * Start a thread to defunct sockets
		 * from apps that are over-using their socket
		 * buffers.
		 */
		if (defunct_tcall == NULL) {
			defunct_tcall =
			    thread_call_allocate_with_options(mbuf_watchdog_defunct,
			    NULL,
			    THREAD_CALL_PRIORITY_KERNEL,
			    THREAD_CALL_OPTIONS_ONCE);
		}
		if (defunct_tcall != NULL) {
			mbuf_watchdog_defunct_active = true;
			thread_call_enter(defunct_tcall);
		}
	}
}

/*
 * Called during blocking allocation.  Returns TRUE if one or more objects
 * are available at the per-CPU caches layer and that allocation should be
 * retried at that level.
 */
static boolean_t
mbuf_sleep(mbuf_class_t class, unsigned int num, int wait)
{
	boolean_t mcache_retry = FALSE;

	LCK_MTX_ASSERT(mbuf_mlock, LCK_MTX_ASSERT_OWNED);

	/* Check if there's anything at the cache layer */
	if (mbuf_cached_above(class, wait)) {
		mcache_retry = TRUE;
		goto done;
	}

	/* Nothing?  Then try hard to get it from somewhere */
	m_reclaim(class, num, (wait & MCR_COMP));

	/* We tried hard and got something? */
	if (m_infree(class) > 0) {
		mbstat.m_wait++;
		goto done;
	} else if (mbuf_cached_above(class, wait)) {
		mbstat.m_wait++;
		mcache_retry = TRUE;
		goto done;
	} else if (wait & MCR_TRYHARD) {
		mcache_retry = TRUE;
		goto done;
	}

	/*
	 * There's really nothing for us right now; inform the
	 * cache(s) that there is a waiter below and go to sleep.
	 */
	mbuf_waiter_inc(class, (wait & MCR_COMP));

	VERIFY(!(wait & MCR_NOSLEEP));

	/*
	 * If this is the first waiter, arm the watchdog timer.  Otherwise
	 * check if we need to panic the system due to watchdog timeout.
	 */
	if (mb_waiters == 0) {
		microuptime(&mb_wdtstart);
	} else {
		mbuf_watchdog();
	}

	mb_waiters++;
	m_region_expand(class) += m_total(class) + num;
	/* wake up the worker thread */
	if (mbuf_worker_ready &&
	    mbuf_worker_needs_wakeup) {
		wakeup((caddr_t)&mbuf_worker_needs_wakeup);
		mbuf_worker_needs_wakeup = FALSE;
	}
	mbwdog_logger("waiting (%d mbufs in class %s)", num, m_cname(class));
	(void) msleep(mb_waitchan, mbuf_mlock, (PZERO - 1), m_cname(class), NULL);
	mbwdog_logger("woke up (%d mbufs in class %s) ", num, m_cname(class));

	/* We are now up; stop getting notified until next round */
	mbuf_waiter_dec(class, (wait & MCR_COMP));

	/* We waited and got something */
	if (m_infree(class) > 0) {
		mbstat.m_wait++;
		goto done;
	} else if (mbuf_cached_above(class, wait)) {
		mbstat.m_wait++;
		mcache_retry = TRUE;
	}
done:
	return mcache_retry;
}

__attribute__((noreturn))
static void
mbuf_worker_thread(void)
{
	int mbuf_expand;

	while (1) {
		lck_mtx_lock(mbuf_mlock);
		mbwdog_logger("worker thread running");
		mbuf_worker_run_cnt++;
		mbuf_expand = 0;
		/*
		 * Allocations are based on page size, so if we have depleted
		 * the reserved spaces, try to free mbufs from the major classes.
		 */
#if PAGE_SIZE == 4096
		uint32_t m_mbclusters = m_total(MC_MBUF) >> NMBPCLSHIFT;
		uint32_t m_clusters = m_total(MC_CL);
		uint32_t m_bigclusters = m_total(MC_BIGCL) << NCLPBGSHIFT;
		uint32_t sumclusters = m_mbclusters + m_clusters + m_bigclusters;
		if (sumclusters >= nclusters) {
			mbwdog_logger("reclaiming bigcl");
			mbuf_drain_locked(TRUE);
			m_reclaim(MC_BIGCL, 4, FALSE);
		}
#else
		uint32_t m_16kclusters = m_total(MC_16KCL);
		if ((m_16kclusters << NCLPJCLSHIFT) >= njcl) {
			mbwdog_logger("reclaiming 16kcl");
			mbuf_drain_locked(TRUE);
			m_reclaim(MC_16KCL, 4, FALSE);
		}
#endif
		if (m_region_expand(MC_CL) > 0) {
			int n;
			mb_expand_cl_cnt++;
			/* Adjust to current number of cluster in use */
			n = m_region_expand(MC_CL) -
			    (m_total(MC_CL) - m_infree(MC_CL));
			if ((n + m_total(MC_CL)) > m_maxlimit(MC_CL)) {
				n = m_maxlimit(MC_CL) - m_total(MC_CL);
			}
			if (n > 0) {
				mb_expand_cl_total += n;
			}
			m_region_expand(MC_CL) = 0;

			if (n > 0) {
				mbwdog_logger("expanding MC_CL by %d", n);
				freelist_populate(MC_CL, n, M_WAIT);
			}
		}
		if (m_region_expand(MC_BIGCL) > 0) {
			int n;
			mb_expand_bigcl_cnt++;
			/* Adjust to current number of 4 KB cluster in use */
			n = m_region_expand(MC_BIGCL) -
			    (m_total(MC_BIGCL) - m_infree(MC_BIGCL));
			if ((n + m_total(MC_BIGCL)) > m_maxlimit(MC_BIGCL)) {
				n = m_maxlimit(MC_BIGCL) - m_total(MC_BIGCL);
			}
			if (n > 0) {
				mb_expand_bigcl_total += n;
			}
			m_region_expand(MC_BIGCL) = 0;

			if (n > 0) {
				mbwdog_logger("expanding MC_BIGCL by %d", n);
				freelist_populate(MC_BIGCL, n, M_WAIT);
			}
		}
		if (m_region_expand(MC_16KCL) > 0) {
			int n;
			mb_expand_16kcl_cnt++;
			/* Adjust to current number of 16 KB cluster in use */
			n = m_region_expand(MC_16KCL) -
			    (m_total(MC_16KCL) - m_infree(MC_16KCL));
			if ((n + m_total(MC_16KCL)) > m_maxlimit(MC_16KCL)) {
				n = m_maxlimit(MC_16KCL) - m_total(MC_16KCL);
			}
			if (n > 0) {
				mb_expand_16kcl_total += n;
			}
			m_region_expand(MC_16KCL) = 0;

			if (n > 0) {
				mbwdog_logger("expanding MC_16KCL by %d", n);
				(void) freelist_populate(MC_16KCL, n, M_WAIT);
			}
		}

		/*
		 * Because we can run out of memory before filling the mbuf
		 * map, we should not allocate more clusters than they are
		 * mbufs -- otherwise we could have a large number of useless
		 * clusters allocated.
		 */
		mbwdog_logger("totals: MC_MBUF %d MC_BIGCL %d MC_CL %d MC_16KCL %d",
		    m_total(MC_MBUF), m_total(MC_BIGCL), m_total(MC_CL),
		    m_total(MC_16KCL));
		uint32_t total_mbufs = m_total(MC_MBUF);
		uint32_t total_clusters = m_total(MC_BIGCL) + m_total(MC_CL) +
		    m_total(MC_16KCL);
		if (total_mbufs < total_clusters) {
			mbwdog_logger("expanding MC_MBUF by %d",
			    total_clusters - total_mbufs);
		}
		while (total_mbufs < total_clusters) {
			mb_expand_cnt++;
			if (freelist_populate(MC_MBUF, 1, M_WAIT) == 0) {
				break;
			}
			total_mbufs = m_total(MC_MBUF);
			total_clusters = m_total(MC_BIGCL) + m_total(MC_CL) +
			    m_total(MC_16KCL);
		}

		mbuf_worker_needs_wakeup = TRUE;
		/*
		 * If there's a deadlock and we're not sending / receiving
		 * packets, net_uptime() won't be updated.  Update it here
		 * so we are sure it's correct.
		 */
		net_update_uptime();
		mbuf_worker_last_runtime = net_uptime();
		assert_wait((caddr_t)&mbuf_worker_needs_wakeup,
		    THREAD_UNINT);
		mbwdog_logger("worker thread sleeping");
		lck_mtx_unlock(mbuf_mlock);
		(void) thread_block((thread_continue_t)mbuf_worker_thread);
	}
}

__attribute__((noreturn))
static void
mbuf_worker_thread_init(void)
{
	mbuf_worker_ready++;
	mbuf_worker_thread();
}

static mcl_slab_t *
slab_get(void *buf)
{
	mcl_slabg_t *slg;
	unsigned int ix, k;

	LCK_MTX_ASSERT(mbuf_mlock, LCK_MTX_ASSERT_OWNED);

	VERIFY(MBUF_IN_MAP(buf));
	ix = ((unsigned char *)buf - mbutl) >> MBSHIFT;
	VERIFY(ix < maxslabgrp);

	if ((slg = slabstbl[ix]) == NULL) {
		/*
		 * In the current implementation, we never shrink the slabs
		 * table; if we attempt to reallocate a cluster group when
		 * it's already allocated, panic since this is a sign of a
		 * memory corruption (slabstbl[ix] got nullified).
		 */
		++slabgrp;
		VERIFY(ix < slabgrp);
		/*
		 * Slabs expansion can only be done single threaded; when
		 * we get here, it must be as a result of m_clalloc() which
		 * is serialized and therefore mb_clalloc_busy must be set.
		 */
		VERIFY(mb_clalloc_busy);
		lck_mtx_unlock(mbuf_mlock);

		/* This is a new buffer; create the slabs group for it */
		slg = zalloc_permanent_type(mcl_slabg_t);
		slg->slg_slab = zalloc_permanent(sizeof(mcl_slab_t) * NSLABSPMB,
		    ZALIGN(mcl_slab_t));

		lck_mtx_lock(mbuf_mlock);
		/*
		 * No other thread could have gone into m_clalloc() after
		 * we dropped the lock above, so verify that it's true.
		 */
		VERIFY(mb_clalloc_busy);

		slabstbl[ix] = slg;

		/* Chain each slab in the group to its forward neighbor */
		for (k = 1; k < NSLABSPMB; k++) {
			slg->slg_slab[k - 1].sl_next = &slg->slg_slab[k];
		}
		VERIFY(slg->slg_slab[NSLABSPMB - 1].sl_next == NULL);

		/* And chain the last slab in the previous group to this */
		if (ix > 0) {
			VERIFY(slabstbl[ix - 1]->
			    slg_slab[NSLABSPMB - 1].sl_next == NULL);
			slabstbl[ix - 1]->slg_slab[NSLABSPMB - 1].sl_next =
			    &slg->slg_slab[0];
		}
	}

	ix = MTOPG(buf) % NSLABSPMB;
	VERIFY(ix < NSLABSPMB);

	return &slg->slg_slab[ix];
}

static void
slab_init(mcl_slab_t *sp, mbuf_class_t class, u_int32_t flags,
    void *base, void *head, unsigned int len, int refcnt, int chunks)
{
	sp->sl_class = class;
	sp->sl_flags = flags;
	sp->sl_base = base;
	sp->sl_head = head;
	sp->sl_len = len;
	sp->sl_refcnt = refcnt;
	sp->sl_chunks = chunks;
	slab_detach(sp);
}

static void
slab_insert(mcl_slab_t *sp, mbuf_class_t class)
{
	VERIFY(slab_is_detached(sp));
	m_slab_cnt(class)++;
	TAILQ_INSERT_TAIL(&m_slablist(class), sp, sl_link);
	sp->sl_flags &= ~SLF_DETACHED;

	/*
	 * If a buffer spans multiple contiguous pages then mark them as
	 * detached too
	 */
	if (class == MC_16KCL) {
		int k;
		for (k = 1; k < NSLABSP16KB; k++) {
			sp = sp->sl_next;
			/* Next slab must already be present */
			VERIFY(sp != NULL && slab_is_detached(sp));
			sp->sl_flags &= ~SLF_DETACHED;
		}
	}
}

static void
slab_remove(mcl_slab_t *sp, mbuf_class_t class)
{
	int k;
	VERIFY(!slab_is_detached(sp));
	VERIFY(m_slab_cnt(class) > 0);
	m_slab_cnt(class)--;
	TAILQ_REMOVE(&m_slablist(class), sp, sl_link);
	slab_detach(sp);
	if (class == MC_16KCL) {
		for (k = 1; k < NSLABSP16KB; k++) {
			sp = sp->sl_next;
			/* Next slab must already be present */
			VERIFY(sp != NULL);
			VERIFY(!slab_is_detached(sp));
			slab_detach(sp);
		}
	}
}

static boolean_t
slab_inrange(mcl_slab_t *sp, void *buf)
{
	return (uintptr_t)buf >= (uintptr_t)sp->sl_base &&
	       (uintptr_t)buf < ((uintptr_t)sp->sl_base + sp->sl_len);
}

#undef panic

static void
slab_nextptr_panic(mcl_slab_t *sp, void *addr)
{
	int i;
	unsigned int chunk_len = sp->sl_len / sp->sl_chunks;
	uintptr_t buf = (uintptr_t)sp->sl_base;

	for (i = 0; i < sp->sl_chunks; i++, buf += chunk_len) {
		void *next = ((mcache_obj_t *)buf)->obj_next;
		if (next != addr) {
			continue;
		}
		if (!mclverify) {
			if (next != NULL && !MBUF_IN_MAP(next)) {
				mcache_t *cp = m_cache(sp->sl_class);
				panic("%s: %s buffer %p in slab %p modified "
				    "after free at offset 0: %p out of range "
				    "[%p-%p)\n", __func__, cp->mc_name,
				    (void *)buf, sp, next, mbutl, embutl);
				/* NOTREACHED */
			}
		} else {
			mcache_audit_t *mca = mcl_audit_buf2mca(sp->sl_class,
			    (mcache_obj_t *)buf);
			mcl_audit_verify_nextptr(next, mca);
		}
	}
}

static void
slab_detach(mcl_slab_t *sp)
{
	sp->sl_link.tqe_next = (mcl_slab_t *)-1;
	sp->sl_link.tqe_prev = (mcl_slab_t **)-1;
	sp->sl_flags |= SLF_DETACHED;
}

static boolean_t
slab_is_detached(mcl_slab_t *sp)
{
	return (intptr_t)sp->sl_link.tqe_next == -1 &&
	       (intptr_t)sp->sl_link.tqe_prev == -1 &&
	       (sp->sl_flags & SLF_DETACHED);
}

static void
mcl_audit_init(void *buf, mcache_audit_t **mca_list,
    mcache_obj_t **con_list, size_t con_size, unsigned int num)
{
	mcache_audit_t *mca, *mca_tail;
	mcache_obj_t *con = NULL;
	boolean_t save_contents = (con_list != NULL);
	unsigned int i, ix;

	ASSERT(num <= NMBPG);
	ASSERT(con_list == NULL || con_size != 0);

	ix = MTOPG(buf);
	VERIFY(ix < maxclaudit);

	/* Make sure we haven't been here before */
	for (i = 0; i < num; i++) {
		VERIFY(mclaudit[ix].cl_audit[i] == NULL);
	}

	mca = mca_tail = *mca_list;
	if (save_contents) {
		con = *con_list;
	}

	for (i = 0; i < num; i++) {
		mcache_audit_t *next;

		next = mca->mca_next;
		bzero(mca, sizeof(*mca));
		mca->mca_next = next;
		mclaudit[ix].cl_audit[i] = mca;

		/* Attach the contents buffer if requested */
		if (save_contents) {
			mcl_saved_contents_t *msc =
			    (mcl_saved_contents_t *)(void *)con;

			VERIFY(msc != NULL);
			VERIFY(IS_P2ALIGNED(msc, sizeof(u_int64_t)));
			VERIFY(con_size == sizeof(*msc));
			mca->mca_contents_size = con_size;
			mca->mca_contents = msc;
			con = con->obj_next;
			bzero(mca->mca_contents, mca->mca_contents_size);
		}

		mca_tail = mca;
		mca = mca->mca_next;
	}

	if (save_contents) {
		*con_list = con;
	}

	*mca_list = mca_tail->mca_next;
	mca_tail->mca_next = NULL;
}

static void
mcl_audit_free(void *buf, unsigned int num)
{
	unsigned int i, ix;
	mcache_audit_t *mca, *mca_list;

	ix = MTOPG(buf);
	VERIFY(ix < maxclaudit);

	if (mclaudit[ix].cl_audit[0] != NULL) {
		mca_list = mclaudit[ix].cl_audit[0];
		for (i = 0; i < num; i++) {
			mca = mclaudit[ix].cl_audit[i];
			mclaudit[ix].cl_audit[i] = NULL;
			if (mca->mca_contents) {
				mcache_free(mcl_audit_con_cache,
				    mca->mca_contents);
			}
		}
		mcache_free_ext(mcache_audit_cache,
		    (mcache_obj_t *)mca_list);
	}
}

/*
 * Given an address of a buffer (mbuf/2KB/4KB/16KB), return
 * the corresponding audit structure for that buffer.
 */
static mcache_audit_t *
mcl_audit_buf2mca(mbuf_class_t class, mcache_obj_t *mobj)
{
	mcache_audit_t *mca = NULL;
	int ix = MTOPG(mobj), m_idx = 0;
	unsigned char *page_addr;

	VERIFY(ix < maxclaudit);
	VERIFY(IS_P2ALIGNED(mobj, MIN(m_maxsize(class), PAGE_SIZE)));

	page_addr = PGTOM(ix);

	switch (class) {
	case MC_MBUF:
		/*
		 * For the mbuf case, find the index of the page
		 * used by the mbuf and use that index to locate the
		 * base address of the page.  Then find out the
		 * mbuf index relative to the page base and use
		 * it to locate the audit structure.
		 */
		m_idx = MBPAGEIDX(page_addr, mobj);
		VERIFY(m_idx < (int)NMBPG);
		mca = mclaudit[ix].cl_audit[m_idx];
		break;

	case MC_CL:
		/*
		 * Same thing as above, but for 2KB clusters in a page.
		 */
		m_idx = CLPAGEIDX(page_addr, mobj);
		VERIFY(m_idx < (int)NCLPG);
		mca = mclaudit[ix].cl_audit[m_idx];
		break;

	case MC_BIGCL:
		m_idx = BCLPAGEIDX(page_addr, mobj);
		VERIFY(m_idx < (int)NBCLPG);
		mca = mclaudit[ix].cl_audit[m_idx];
		break;
	case MC_16KCL:
		/*
		 * Same as above, but only return the first element.
		 */
		mca = mclaudit[ix].cl_audit[0];
		break;

	default:
		VERIFY(0);
		/* NOTREACHED */
	}

	return mca;
}

static void
mcl_audit_mbuf(mcache_audit_t *mca, void *addr, boolean_t composite,
    boolean_t alloc)
{
	struct mbuf *m = addr;
	mcache_obj_t *next = ((mcache_obj_t *)m)->obj_next;

	VERIFY(mca->mca_contents != NULL &&
	    mca->mca_contents_size == AUDIT_CONTENTS_SIZE);

	if (mclverify) {
		mcl_audit_verify_nextptr(next, mca);
	}

	if (!alloc) {
		/* Save constructed mbuf fields */
		mcl_audit_save_mbuf(m, mca);
		if (mclverify) {
			mcache_set_pattern(MCACHE_FREE_PATTERN, m,
			    m_maxsize(MC_MBUF));
		}
		((mcache_obj_t *)m)->obj_next = next;
		return;
	}

	/* Check if the buffer has been corrupted while in freelist */
	if (mclverify) {
		mcache_audit_free_verify_set(mca, addr, 0, m_maxsize(MC_MBUF));
	}
	/* Restore constructed mbuf fields */
	mcl_audit_restore_mbuf(m, mca, composite);
}

static void
mcl_audit_restore_mbuf(struct mbuf *m, mcache_audit_t *mca, boolean_t composite)
{
	struct mbuf *ms = MCA_SAVED_MBUF_PTR(mca);

	if (composite) {
		struct mbuf *next = m->m_next;
		VERIFY(ms->m_flags == M_EXT && m_get_rfa(ms) != NULL &&
		    MBUF_IS_COMPOSITE(ms));
		VERIFY(mca->mca_contents_size == AUDIT_CONTENTS_SIZE);
		/*
		 * We could have hand-picked the mbuf fields and restore
		 * them individually, but that will be a maintenance
		 * headache.  Instead, restore everything that was saved;
		 * the mbuf layer will recheck and reinitialize anyway.
		 */
		bcopy(ms, m, MCA_SAVED_MBUF_SIZE);
		m->m_next = next;
	} else {
		/*
		 * For a regular mbuf (no cluster attached) there's nothing
		 * to restore other than the type field, which is expected
		 * to be MT_FREE.
		 */
		m->m_type = ms->m_type;
	}
	mbuf_mcheck(m);
}

static void
mcl_audit_save_mbuf(struct mbuf *m, mcache_audit_t *mca)
{
	VERIFY(mca->mca_contents_size == AUDIT_CONTENTS_SIZE);
	mbuf_mcheck(m);
	bcopy(m, MCA_SAVED_MBUF_PTR(mca), MCA_SAVED_MBUF_SIZE);
}

static void
mcl_audit_cluster(mcache_audit_t *mca, void *addr, size_t size, boolean_t alloc,
    boolean_t save_next)
{
	mcache_obj_t *next = ((mcache_obj_t *)addr)->obj_next;

	if (!alloc) {
		if (mclverify) {
			mcache_set_pattern(MCACHE_FREE_PATTERN, addr, size);
		}
		if (save_next) {
			mcl_audit_verify_nextptr(next, mca);
			((mcache_obj_t *)addr)->obj_next = next;
		}
	} else if (mclverify) {
		/* Check if the buffer has been corrupted while in freelist */
		mcl_audit_verify_nextptr(next, mca);
		mcache_audit_free_verify_set(mca, addr, 0, size);
	}
}

static void
mcl_audit_scratch(mcache_audit_t *mca)
{
	void *stack[MCACHE_STACK_DEPTH + 1];
	mcl_scratch_audit_t *msa;
	struct timeval now;

	VERIFY(mca->mca_contents != NULL);
	msa = MCA_SAVED_SCRATCH_PTR(mca);

	msa->msa_pthread = msa->msa_thread;
	msa->msa_thread = current_thread();
	bcopy(msa->msa_stack, msa->msa_pstack, sizeof(msa->msa_pstack));
	msa->msa_pdepth = msa->msa_depth;
	bzero(stack, sizeof(stack));
	msa->msa_depth = OSBacktrace(stack, MCACHE_STACK_DEPTH + 1) - 1;
	bcopy(&stack[1], msa->msa_stack, sizeof(msa->msa_stack));

	msa->msa_ptstamp = msa->msa_tstamp;
	microuptime(&now);
	/* tstamp is in ms relative to base_ts */
	msa->msa_tstamp = ((now.tv_usec - mb_start.tv_usec) / 1000);
	if ((now.tv_sec - mb_start.tv_sec) > 0) {
		msa->msa_tstamp += ((now.tv_sec - mb_start.tv_sec) * 1000);
	}
}

__abortlike
static void
mcl_audit_mcheck_panic(struct mbuf *m)
{
	char buf[DUMP_MCA_BUF_SIZE];
	mcache_audit_t *mca;

	MRANGE(m);
	mca = mcl_audit_buf2mca(MC_MBUF, (mcache_obj_t *)m);

	panic("mcl_audit: freed mbuf %p with type 0x%x (instead of 0x%x)\n%s",
	    m, (u_int16_t)m->m_type, MT_FREE, mcache_dump_mca(buf, mca));
	/* NOTREACHED */
}

__abortlike
static void
mcl_audit_verify_nextptr_panic(void *next, mcache_audit_t *mca)
{
	char buf[DUMP_MCA_BUF_SIZE];
	panic("mcl_audit: buffer %p modified after free at offset 0: "
	    "%p out of range [%p-%p)\n%s\n",
	    mca->mca_addr, next, mbutl, embutl, mcache_dump_mca(buf, mca));
	/* NOTREACHED */
}

static void
mcl_audit_verify_nextptr(void *next, mcache_audit_t *mca)
{
	if (next != NULL && !MBUF_IN_MAP(next) &&
	    (next != (void *)MCACHE_FREE_PATTERN || !mclverify)) {
		mcl_audit_verify_nextptr_panic(next, mca);
	}
}

static uintptr_t
hash_mix(uintptr_t x)
{
#ifndef __LP64__
	x += ~(x << 15);
	x ^=  (x >> 10);
	x +=  (x << 3);
	x ^=  (x >> 6);
	x += ~(x << 11);
	x ^=  (x >> 16);
#else
	x += ~(x << 32);
	x ^=  (x >> 22);
	x += ~(x << 13);
	x ^=  (x >> 8);
	x +=  (x << 3);
	x ^=  (x >> 15);
	x += ~(x << 27);
	x ^=  (x >> 31);
#endif
	return x;
}

static uint32_t
hashbacktrace(uintptr_t* bt, uint32_t depth, uint32_t max_size)
{
	uintptr_t hash = 0;
	uintptr_t mask = max_size - 1;

	while (depth) {
		hash += bt[--depth];
	}

	hash = hash_mix(hash) & mask;

	assert(hash < max_size);

	return (uint32_t) hash;
}

static uint32_t
hashaddr(uintptr_t pt, uint32_t max_size)
{
	uintptr_t hash = 0;
	uintptr_t mask = max_size - 1;

	hash = hash_mix(pt) & mask;

	assert(hash < max_size);

	return (uint32_t) hash;
}

/* This function turns on mbuf leak detection */
static void
mleak_activate(void)
{
	mleak_table.mleak_sample_factor = MLEAK_SAMPLE_FACTOR;
	PE_parse_boot_argn("mleak_sample_factor",
	    &mleak_table.mleak_sample_factor,
	    sizeof(mleak_table.mleak_sample_factor));

	if (mleak_table.mleak_sample_factor == 0) {
		mclfindleak = 0;
	}

	if (mclfindleak == 0) {
		return;
	}

	vm_size_t alloc_size =
	    mleak_alloc_buckets * sizeof(struct mallocation);
	vm_size_t trace_size = mleak_trace_buckets * sizeof(struct mtrace);

	mleak_allocations = zalloc_permanent(alloc_size, ZALIGN(struct mallocation));
	mleak_traces = zalloc_permanent(trace_size, ZALIGN(struct mtrace));
	mleak_stat = zalloc_permanent(MLEAK_STAT_SIZE(MLEAK_NUM_TRACES),
	    ZALIGN(mleak_stat_t));

	mleak_stat->ml_cnt = MLEAK_NUM_TRACES;
#ifdef __LP64__
	mleak_stat->ml_isaddr64 = 1;
#endif /* __LP64__ */
}

static void
mleak_logger(u_int32_t num, mcache_obj_t *addr, boolean_t alloc)
{
	int temp;

	if (mclfindleak == 0) {
		return;
	}

	if (!alloc) {
		return mleak_free(addr);
	}

	temp = os_atomic_inc_orig(&mleak_table.mleak_capture, relaxed);

	if ((temp % mleak_table.mleak_sample_factor) == 0 && addr != NULL) {
		uintptr_t bt[MLEAK_STACK_DEPTH];
		unsigned int logged = backtrace(bt, MLEAK_STACK_DEPTH, NULL, NULL);
		mleak_log(bt, addr, logged, num);
	}
}

/*
 * This function records the allocation in the mleak_allocations table
 * and the backtrace in the mleak_traces table; if allocation slot is in use,
 * replace old allocation with new one if the trace slot is in use, return
 * (or increment refcount if same trace).
 */
static boolean_t
mleak_log(uintptr_t *bt, mcache_obj_t *addr, uint32_t depth, int num)
{
	struct mallocation *allocation;
	struct mtrace *trace;
	uint32_t trace_index;

	/* Quit if someone else modifying the tables */
	if (!lck_mtx_try_lock_spin(mleak_lock)) {
		mleak_table.total_conflicts++;
		return FALSE;
	}

	allocation = &mleak_allocations[hashaddr((uintptr_t)addr,
	    mleak_alloc_buckets)];
	trace_index = hashbacktrace(bt, depth, mleak_trace_buckets);
	trace = &mleak_traces[trace_index];

	VERIFY(allocation <= &mleak_allocations[mleak_alloc_buckets - 1]);
	VERIFY(trace <= &mleak_traces[mleak_trace_buckets - 1]);

	allocation->hitcount++;
	trace->hitcount++;

	/*
	 * If the allocation bucket we want is occupied
	 * and the occupier has the same trace, just bail.
	 */
	if (allocation->element != NULL &&
	    trace_index == allocation->trace_index) {
		mleak_table.alloc_collisions++;
		lck_mtx_unlock(mleak_lock);
		return TRUE;
	}

	/*
	 * Store the backtrace in the traces array;
	 * Size of zero = trace bucket is free.
	 */
	if (trace->allocs > 0 &&
	    bcmp(trace->addr, bt, (depth * sizeof(uintptr_t))) != 0) {
		/* Different, unique trace, but the same hash! Bail out. */
		trace->collisions++;
		mleak_table.trace_collisions++;
		lck_mtx_unlock(mleak_lock);
		return TRUE;
	} else if (trace->allocs > 0) {
		/* Same trace, already added, so increment refcount */
		trace->allocs++;
	} else {
		/* Found an unused trace bucket, so record the trace here */
		if (trace->depth != 0) {
			/* this slot previously used but not currently in use */
			mleak_table.trace_overwrites++;
		}
		mleak_table.trace_recorded++;
		trace->allocs = 1;
		memcpy(trace->addr, bt, (depth * sizeof(uintptr_t)));
		trace->depth = depth;
		trace->collisions = 0;
	}

	/* Step 2: Store the allocation record in the allocations array */
	if (allocation->element != NULL) {
		/*
		 * Replace an existing allocation.  No need to preserve
		 * because only a subset of the allocations are being
		 * recorded anyway.
		 */
		mleak_table.alloc_collisions++;
	} else if (allocation->trace_index != 0) {
		mleak_table.alloc_overwrites++;
	}
	allocation->element = addr;
	allocation->trace_index = trace_index;
	allocation->count = num;
	mleak_table.alloc_recorded++;
	mleak_table.outstanding_allocs++;

	lck_mtx_unlock(mleak_lock);
	return TRUE;
}

static void
mleak_free(mcache_obj_t *addr)
{
	while (addr != NULL) {
		struct mallocation *allocation = &mleak_allocations
		    [hashaddr((uintptr_t)addr, mleak_alloc_buckets)];

		if (allocation->element == addr &&
		    allocation->trace_index < mleak_trace_buckets) {
			lck_mtx_lock_spin(mleak_lock);
			if (allocation->element == addr &&
			    allocation->trace_index < mleak_trace_buckets) {
				struct mtrace *trace;
				trace = &mleak_traces[allocation->trace_index];
				/* allocs = 0 means trace bucket is unused */
				if (trace->allocs > 0) {
					trace->allocs--;
				}
				if (trace->allocs == 0) {
					trace->depth = 0;
				}
				/* NULL element means alloc bucket is unused */
				allocation->element = NULL;
				mleak_table.outstanding_allocs--;
			}
			lck_mtx_unlock(mleak_lock);
		}
		addr = addr->obj_next;
	}
}

static void
mleak_sort_traces()
{
	int i, j, k;
	struct mtrace *swap;

	for (i = 0; i < MLEAK_NUM_TRACES; i++) {
		mleak_top_trace[i] = NULL;
	}

	for (i = 0, j = 0; j < MLEAK_NUM_TRACES && i < mleak_trace_buckets; i++) {
		if (mleak_traces[i].allocs <= 0) {
			continue;
		}

		mleak_top_trace[j] = &mleak_traces[i];
		for (k = j; k > 0; k--) {
			if (mleak_top_trace[k]->allocs <=
			    mleak_top_trace[k - 1]->allocs) {
				break;
			}

			swap = mleak_top_trace[k - 1];
			mleak_top_trace[k - 1] = mleak_top_trace[k];
			mleak_top_trace[k] = swap;
		}
		j++;
	}

	j--;
	for (; i < mleak_trace_buckets; i++) {
		if (mleak_traces[i].allocs <= mleak_top_trace[j]->allocs) {
			continue;
		}

		mleak_top_trace[j] = &mleak_traces[i];

		for (k = j; k > 0; k--) {
			if (mleak_top_trace[k]->allocs <=
			    mleak_top_trace[k - 1]->allocs) {
				break;
			}

			swap = mleak_top_trace[k - 1];
			mleak_top_trace[k - 1] = mleak_top_trace[k];
			mleak_top_trace[k] = swap;
		}
	}
}

static void
mleak_update_stats()
{
	mleak_trace_stat_t *mltr;
	int i;

	VERIFY(mleak_stat != NULL);
#ifdef __LP64__
	VERIFY(mleak_stat->ml_isaddr64);
#else
	VERIFY(!mleak_stat->ml_isaddr64);
#endif /* !__LP64__ */
	VERIFY(mleak_stat->ml_cnt == MLEAK_NUM_TRACES);

	mleak_sort_traces();

	mltr = &mleak_stat->ml_trace[0];
	bzero(mltr, sizeof(*mltr) * MLEAK_NUM_TRACES);
	for (i = 0; i < MLEAK_NUM_TRACES; i++) {
		int j;

		if (mleak_top_trace[i] == NULL ||
		    mleak_top_trace[i]->allocs == 0) {
			continue;
		}

		mltr->mltr_collisions   = mleak_top_trace[i]->collisions;
		mltr->mltr_hitcount     = mleak_top_trace[i]->hitcount;
		mltr->mltr_allocs       = mleak_top_trace[i]->allocs;
		mltr->mltr_depth        = mleak_top_trace[i]->depth;

		VERIFY(mltr->mltr_depth <= MLEAK_STACK_DEPTH);
		for (j = 0; j < mltr->mltr_depth; j++) {
			mltr->mltr_addr[j] = mleak_top_trace[i]->addr[j];
		}

		mltr++;
	}
}

static struct mbtypes {
	int             mt_type;
	const char      *mt_name;
} mbtypes[] = {
	{ MT_DATA, "data" },
	{ MT_OOBDATA, "oob data" },
	{ MT_CONTROL, "ancillary data" },
	{ MT_HEADER, "packet headers" },
	{ MT_SOCKET, "socket structures" },
	{ MT_PCB, "protocol control blocks" },
	{ MT_RTABLE, "routing table entries" },
	{ MT_HTABLE, "IMP host table entries" },
	{ MT_ATABLE, "address resolution tables" },
	{ MT_FTABLE, "fragment reassembly queue headers" },
	{ MT_SONAME, "socket names and addresses" },
	{ MT_SOOPTS, "socket options" },
	{ MT_RIGHTS, "access rights" },
	{ MT_IFADDR, "interface addresses" },
	{ MT_TAG, "packet tags" },
	{ 0, NULL }
};

#define MBUF_DUMP_BUF_CHK() {   \
	clen -= k;              \
	if (clen < 1)           \
	        goto done;      \
	c += k;                 \
}

static char *
mbuf_dump(void)
{
	unsigned long totmem = 0, totfree = 0, totmbufs, totused, totpct,
	    totreturned = 0;
	u_int32_t m_mbufs = 0, m_clfree = 0, m_bigclfree = 0;
	u_int32_t m_mbufclfree = 0, m_mbufbigclfree = 0;
	u_int32_t m_16kclusters = 0, m_16kclfree = 0, m_mbuf16kclfree = 0;
	int nmbtypes = sizeof(mbstat.m_mtypes) / sizeof(short);
	uint8_t seen[256];
	struct mbtypes *mp;
	mb_class_stat_t *sp;
	mleak_trace_stat_t *mltr;
	char *c = mbuf_dump_buf;
	int i, j, k, clen = MBUF_DUMP_BUF_SIZE;
	struct mbuf_watchdog_defunct_args args = {};

	mbuf_dump_buf[0] = '\0';

	/* synchronize all statistics in the mbuf table */
	mbuf_stat_sync();
	mbuf_mtypes_sync();

	sp = &mb_stat->mbs_class[0];
	for (i = 0; i < mb_stat->mbs_cnt; i++, sp++) {
		u_int32_t mem;

		if (m_class(i) == MC_MBUF) {
			m_mbufs = sp->mbcl_active;
		} else if (m_class(i) == MC_CL) {
			m_clfree = sp->mbcl_total - sp->mbcl_active;
		} else if (m_class(i) == MC_BIGCL) {
			m_bigclfree = sp->mbcl_total - sp->mbcl_active;
		} else if (m_class(i) == MC_16KCL) {
			m_16kclfree = sp->mbcl_total - sp->mbcl_active;
			m_16kclusters = sp->mbcl_total;
		} else if (m_class(i) == MC_MBUF_CL) {
			m_mbufclfree = sp->mbcl_total - sp->mbcl_active;
		} else if (m_class(i) == MC_MBUF_BIGCL) {
			m_mbufbigclfree = sp->mbcl_total - sp->mbcl_active;
		} else if (m_class(i) == MC_MBUF_16KCL) {
			m_mbuf16kclfree = sp->mbcl_total - sp->mbcl_active;
		}

		mem = sp->mbcl_ctotal * sp->mbcl_size;
		totmem += mem;
		totfree += (sp->mbcl_mc_cached + sp->mbcl_infree) *
		    sp->mbcl_size;
		totreturned += sp->mbcl_release_cnt;
	}

	/* adjust free counts to include composite caches */
	m_clfree += m_mbufclfree;
	m_bigclfree += m_mbufbigclfree;
	m_16kclfree += m_mbuf16kclfree;

	totmbufs = 0;
	for (mp = mbtypes; mp->mt_name != NULL; mp++) {
		totmbufs += mbstat.m_mtypes[mp->mt_type];
	}
	if (totmbufs > m_mbufs) {
		totmbufs = m_mbufs;
	}
	k = scnprintf(c, clen, "%lu/%u mbufs in use:\n", totmbufs, m_mbufs);
	MBUF_DUMP_BUF_CHK();

	bzero(&seen, sizeof(seen));
	for (mp = mbtypes; mp->mt_name != NULL; mp++) {
		if (mbstat.m_mtypes[mp->mt_type] != 0) {
			seen[mp->mt_type] = 1;
			k = scnprintf(c, clen, "\t%u mbufs allocated to %s\n",
			    mbstat.m_mtypes[mp->mt_type], mp->mt_name);
			MBUF_DUMP_BUF_CHK();
		}
	}
	seen[MT_FREE] = 1;
	for (i = 0; i < nmbtypes; i++) {
		if (!seen[i] && mbstat.m_mtypes[i] != 0) {
			k = scnprintf(c, clen, "\t%u mbufs allocated to "
			    "<mbuf type %d>\n", mbstat.m_mtypes[i], i);
			MBUF_DUMP_BUF_CHK();
		}
	}
	if ((m_mbufs - totmbufs) > 0) {
		k = scnprintf(c, clen, "\t%lu mbufs allocated to caches\n",
		    m_mbufs - totmbufs);
		MBUF_DUMP_BUF_CHK();
	}
	k = scnprintf(c, clen, "%u/%u mbuf 2KB clusters in use\n"
	    "%u/%u mbuf 4KB clusters in use\n",
	    (unsigned int)(mbstat.m_clusters - m_clfree),
	    (unsigned int)mbstat.m_clusters,
	    (unsigned int)(mbstat.m_bigclusters - m_bigclfree),
	    (unsigned int)mbstat.m_bigclusters);
	MBUF_DUMP_BUF_CHK();

	k = scnprintf(c, clen, "%u/%u mbuf %uKB clusters in use\n",
	    m_16kclusters - m_16kclfree, m_16kclusters,
	    njclbytes / 1024);
	MBUF_DUMP_BUF_CHK();
	totused = totmem - totfree;
	if (totmem == 0) {
		totpct = 0;
	} else if (totused < (ULONG_MAX / 100)) {
		totpct = (totused * 100) / totmem;
	} else {
		u_long totmem1 = totmem / 100;
		u_long totused1 = totused / 100;
		totpct = (totused1 * 100) / totmem1;
	}
	k = scnprintf(c, clen, "%lu KB allocated to network (approx. %lu%% "
	    "in use)\n", totmem / 1024, totpct);
	MBUF_DUMP_BUF_CHK();
	k = scnprintf(c, clen, "%lu KB returned to the system\n",
	    totreturned / 1024);
	MBUF_DUMP_BUF_CHK();

	net_update_uptime();

	k = scnprintf(c, clen,
	    "worker thread runs: %u, expansions: %llu, cl %llu/%llu, "
	    "bigcl %llu/%llu, 16k %llu/%llu\n", mbuf_worker_run_cnt,
	    mb_expand_cnt, mb_expand_cl_cnt, mb_expand_cl_total,
	    mb_expand_bigcl_cnt, mb_expand_bigcl_total, mb_expand_16kcl_cnt,
	    mb_expand_16kcl_total);
	MBUF_DUMP_BUF_CHK();
	if (mbuf_worker_last_runtime != 0) {
		k = scnprintf(c, clen, "worker thread last run time: "
		    "%llu (%llu seconds ago)\n",
		    mbuf_worker_last_runtime,
		    net_uptime() - mbuf_worker_last_runtime);
		MBUF_DUMP_BUF_CHK();
	}
	if (mbuf_drain_last_runtime != 0) {
		k = scnprintf(c, clen, "drain routine last run time: "
		    "%llu (%llu seconds ago)\n",
		    mbuf_drain_last_runtime,
		    net_uptime() - mbuf_drain_last_runtime);
		MBUF_DUMP_BUF_CHK();
	}

	/*
	 * Log where the most mbufs have accumulated:
	 * - Process socket buffers
	 * - TCP reassembly queue
	 * - Interface AQM queue (output) and DLIL input queue
	 */
	args.non_blocking = true;
	proc_iterate(PROC_ALLPROCLIST,
	    mbuf_watchdog_defunct_iterate, &args, NULL, NULL);
	if (args.top_app != NULL) {
		k = scnprintf(c, clen, "\ntop proc mbuf space %u bytes by %s:%d\n",
		    args.top_app_space_used,
		    proc_name_address(args.top_app),
		    proc_pid(args.top_app));
		proc_rele(args.top_app);
	}
	MBUF_DUMP_BUF_CHK();

#if INET
	k = dump_tcp_reass_qlen(c, clen);
	MBUF_DUMP_BUF_CHK();
#endif /* INET */

#if MPTCP
	k = dump_mptcp_reass_qlen(c, clen);
	MBUF_DUMP_BUF_CHK();
#endif /* MPTCP */

#if NETWORKING
	k = dlil_dump_top_if_qlen(c, clen);
	MBUF_DUMP_BUF_CHK();
#endif /* NETWORKING */

	/* mbuf leak detection statistics */
	mleak_update_stats();

	k = scnprintf(c, clen, "\nmbuf leak detection table:\n");
	MBUF_DUMP_BUF_CHK();
	k = scnprintf(c, clen, "\ttotal captured: %u (one per %u)\n",
	    mleak_table.mleak_capture / mleak_table.mleak_sample_factor,
	    mleak_table.mleak_sample_factor);
	MBUF_DUMP_BUF_CHK();
	k = scnprintf(c, clen, "\ttotal allocs outstanding: %llu\n",
	    mleak_table.outstanding_allocs);
	MBUF_DUMP_BUF_CHK();
	k = scnprintf(c, clen, "\tnew hash recorded: %llu allocs, %llu traces\n",
	    mleak_table.alloc_recorded, mleak_table.trace_recorded);
	MBUF_DUMP_BUF_CHK();
	k = scnprintf(c, clen, "\thash collisions: %llu allocs, %llu traces\n",
	    mleak_table.alloc_collisions, mleak_table.trace_collisions);
	MBUF_DUMP_BUF_CHK();
	k = scnprintf(c, clen, "\toverwrites: %llu allocs, %llu traces\n",
	    mleak_table.alloc_overwrites, mleak_table.trace_overwrites);
	MBUF_DUMP_BUF_CHK();
	k = scnprintf(c, clen, "\tlock conflicts: %llu\n\n",
	    mleak_table.total_conflicts);
	MBUF_DUMP_BUF_CHK();

	k = scnprintf(c, clen, "top %d outstanding traces:\n",
	    mleak_stat->ml_cnt);
	MBUF_DUMP_BUF_CHK();
	for (i = 0; i < mleak_stat->ml_cnt; i++) {
		mltr = &mleak_stat->ml_trace[i];
		k = scnprintf(c, clen, "[%d] %llu outstanding alloc(s), "
		    "%llu hit(s), %llu collision(s)\n", (i + 1),
		    mltr->mltr_allocs, mltr->mltr_hitcount,
		    mltr->mltr_collisions);
		MBUF_DUMP_BUF_CHK();
	}

	if (mleak_stat->ml_isaddr64) {
		k = scnprintf(c, clen, MB_LEAK_HDR_64);
	} else {
		k = scnprintf(c, clen, MB_LEAK_HDR_32);
	}
	MBUF_DUMP_BUF_CHK();

	for (i = 0; i < MLEAK_STACK_DEPTH; i++) {
		k = scnprintf(c, clen, "%2d: ", (i + 1));
		MBUF_DUMP_BUF_CHK();
		for (j = 0; j < mleak_stat->ml_cnt; j++) {
			mltr = &mleak_stat->ml_trace[j];
			if (i < mltr->mltr_depth) {
				if (mleak_stat->ml_isaddr64) {
					k = scnprintf(c, clen, "0x%0llx  ",
					    (uint64_t)VM_KERNEL_UNSLIDE(
						    mltr->mltr_addr[i]));
				} else {
					k = scnprintf(c, clen,
					    "0x%08x  ",
					    (uint32_t)VM_KERNEL_UNSLIDE(
						    mltr->mltr_addr[i]));
				}
			} else {
				if (mleak_stat->ml_isaddr64) {
					k = scnprintf(c, clen,
					    MB_LEAK_SPACING_64);
				} else {
					k = scnprintf(c, clen,
					    MB_LEAK_SPACING_32);
				}
			}
			MBUF_DUMP_BUF_CHK();
		}
		k = scnprintf(c, clen, "\n");
		MBUF_DUMP_BUF_CHK();
	}

done:
	return mbuf_dump_buf;
}

#undef MBUF_DUMP_BUF_CHK

/*
 * This routine is reserved for mbuf_get_driver_scratch(); clients inside
 * xnu that intend on utilizing the module-private area should directly
 * refer to the pkt_mpriv structure in the pkthdr.  They are also expected
 * to set and clear PKTF_PRIV_GUARDED, while owning the packet and prior
 * to handing it off to another module, respectively.
 */
u_int32_t
m_scratch_get(struct mbuf *m, u_int8_t **p)
{
	struct pkthdr *pkt = &m->m_pkthdr;

	VERIFY(m->m_flags & M_PKTHDR);

	/* See comments in <rdar://problem/14040693> */
	if (pkt->pkt_flags & PKTF_PRIV_GUARDED) {
		panic_plain("Invalid attempt to access guarded module-private "
		    "area: mbuf %p, pkt_flags 0x%x\n", m, pkt->pkt_flags);
		/* NOTREACHED */
	}

	if (mcltrace) {
		mcache_audit_t *mca;

		lck_mtx_lock(mbuf_mlock);
		mca = mcl_audit_buf2mca(MC_MBUF, (mcache_obj_t *)m);
		if (mca->mca_uflags & MB_SCVALID) {
			mcl_audit_scratch(mca);
		}
		lck_mtx_unlock(mbuf_mlock);
	}

	*p = (u_int8_t *)&pkt->pkt_mpriv;
	return sizeof(pkt->pkt_mpriv);
}

/*
 * Simple routine to avoid taking the lock when we can't run the
 * mbuf drain.
 */
static int
mbuf_drain_checks(boolean_t ignore_waiters)
{
	if (mb_drain_maxint == 0) {
		return 0;
	}
	if (!ignore_waiters && mb_waiters != 0) {
		return 0;
	}

	return 1;
}

/*
 * Called by the VM when there's memory pressure or when we exhausted
 * the 4k/16k reserved space.
 */
static void
mbuf_drain_locked(boolean_t ignore_waiters)
{
	mbuf_class_t mc;
	mcl_slab_t *sp, *sp_tmp, *nsp;
	unsigned int num, k, interval, released = 0;
	unsigned long total_mem = 0, use_mem = 0;
	boolean_t ret, purge_caches = FALSE;
	ppnum_t offset;
	mcache_obj_t *obj;
	unsigned long per;
	static unsigned char scratch[32];
	static ppnum_t scratch_pa = 0;

	LCK_MTX_ASSERT(mbuf_mlock, LCK_MTX_ASSERT_OWNED);
	if (!mbuf_drain_checks(ignore_waiters)) {
		return;
	}
	if (scratch_pa == 0) {
		bzero(scratch, sizeof(scratch));
		scratch_pa = pmap_find_phys(kernel_pmap, (addr64_t)scratch);
		VERIFY(scratch_pa);
	} else if (mclverify) {
		/*
		 * Panic if a driver wrote to our scratch memory.
		 */
		for (k = 0; k < sizeof(scratch); k++) {
			if (scratch[k]) {
				panic("suspect DMA to freed address");
			}
		}
	}
	/*
	 * Don't free memory too often as that could cause excessive
	 * waiting times for mbufs.  Purge caches if we were asked to drain
	 * in the last 5 minutes.
	 */
	if (mbuf_drain_last_runtime != 0) {
		interval = net_uptime() - mbuf_drain_last_runtime;
		if (interval <= mb_drain_maxint) {
			return;
		}
		if (interval <= mb_drain_maxint * 5) {
			purge_caches = TRUE;
		}
	}
	mbuf_drain_last_runtime = net_uptime();
	/*
	 * Don't free any memory if we're using 60% or more.
	 */
	for (mc = 0; mc < MC_MAX; mc++) {
		total_mem += m_total(mc) * m_maxsize(mc);
		use_mem += m_active(mc) * m_maxsize(mc);
	}
	per = (use_mem * 100) / total_mem;
	if (per >= 60) {
		return;
	}
	/*
	 * Purge all the caches.  This effectively disables
	 * caching for a few seconds, but the mbuf worker thread will
	 * re-enable them again.
	 */
	if (purge_caches == TRUE) {
		for (mc = 0; mc < MC_MAX; mc++) {
			if (m_total(mc) < m_avgtotal(mc)) {
				continue;
			}
			lck_mtx_unlock(mbuf_mlock);
			ret = mcache_purge_cache(m_cache(mc), FALSE);
			lck_mtx_lock(mbuf_mlock);
			if (ret == TRUE) {
				m_purge_cnt(mc)++;
			}
		}
	}
	/*
	 * Move the objects from the composite class freelist to
	 * the rudimentary slabs list, but keep at least 10% of the average
	 * total in the freelist.
	 */
	for (mc = 0; mc < MC_MAX; mc++) {
		while (m_cobjlist(mc) &&
		    m_total(mc) < m_avgtotal(mc) &&
		    m_infree(mc) > 0.1 * m_avgtotal(mc) + m_minlimit(mc)) {
			obj = m_cobjlist(mc);
			m_cobjlist(mc) = obj->obj_next;
			obj->obj_next = NULL;
			num = cslab_free(mc, obj, 1);
			VERIFY(num == 1);
			m_free_cnt(mc)++;
			m_infree(mc)--;
			/* cslab_free() handles m_total */
		}
	}
	/*
	 * Free the buffers present in the slab list up to 10% of the total
	 * average per class.
	 *
	 * We walk the list backwards in an attempt to reduce fragmentation.
	 */
	for (mc = MC_MAX - 1; (int)mc >= 0; mc--) {
		TAILQ_FOREACH_SAFE(sp, &m_slablist(mc), sl_link, sp_tmp) {
			/*
			 * Process only unused slabs occupying memory.
			 */
			if (sp->sl_refcnt != 0 || sp->sl_len == 0 ||
			    sp->sl_base == NULL) {
				continue;
			}
			if (m_total(mc) < m_avgtotal(mc) ||
			    m_infree(mc) < 0.1 * m_avgtotal(mc) + m_minlimit(mc)) {
				break;
			}
			slab_remove(sp, mc);
			switch (mc) {
			case MC_MBUF:
				m_infree(mc) -= NMBPG;
				m_total(mc) -= NMBPG;
				if (mclaudit != NULL) {
					mcl_audit_free(sp->sl_base, NMBPG);
				}
				break;
			case MC_CL:
				m_infree(mc) -= NCLPG;
				m_total(mc) -= NCLPG;
				if (mclaudit != NULL) {
					mcl_audit_free(sp->sl_base, NMBPG);
				}
				break;
			case MC_BIGCL:
			{
				m_infree(mc) -= NBCLPG;
				m_total(mc) -= NBCLPG;
				if (mclaudit != NULL) {
					mcl_audit_free(sp->sl_base, NMBPG);
				}
				break;
			}
			case MC_16KCL:
				m_infree(mc)--;
				m_total(mc)--;
				for (nsp = sp, k = 1; k < NSLABSP16KB; k++) {
					nsp = nsp->sl_next;
					VERIFY(nsp->sl_refcnt == 0 &&
					    nsp->sl_base != NULL &&
					    nsp->sl_len == 0);
					slab_init(nsp, 0, 0, NULL, NULL, 0, 0,
					    0);
					nsp->sl_flags = 0;
				}
				if (mclaudit != NULL) {
					if (sp->sl_len == PAGE_SIZE) {
						mcl_audit_free(sp->sl_base,
						    NMBPG);
					} else {
						mcl_audit_free(sp->sl_base, 1);
					}
				}
				break;
			default:
				/*
				 * The composite classes have their own
				 * freelist (m_cobjlist), so we only
				 * process rudimentary classes here.
				 */
				VERIFY(0);
			}
			m_release_cnt(mc) += m_size(mc);
			released += m_size(mc);
			VERIFY(sp->sl_base != NULL &&
			    sp->sl_len >= PAGE_SIZE);
			offset = MTOPG(sp->sl_base);
			/*
			 * Make sure the IOMapper points to a valid, but
			 * bogus, address.  This should prevent further DMA
			 * accesses to freed memory.
			 */
			IOMapperInsertPage(mcl_paddr_base, offset, scratch_pa);
			mcl_paddr[offset] = 0;
			kmem_free(mb_map, (vm_offset_t)sp->sl_base,
			    sp->sl_len);
			slab_init(sp, 0, 0, NULL, NULL, 0, 0, 0);
			sp->sl_flags = 0;
		}
	}
	mbstat.m_drain++;
	mbstat.m_bigclusters = m_total(MC_BIGCL);
	mbstat.m_clusters = m_total(MC_CL);
	mbstat.m_mbufs = m_total(MC_MBUF);
	mbuf_stat_sync();
	mbuf_mtypes_sync();
}

__private_extern__ void
mbuf_drain(boolean_t ignore_waiters)
{
	LCK_MTX_ASSERT(mbuf_mlock, LCK_MTX_ASSERT_NOTOWNED);
	if (!mbuf_drain_checks(ignore_waiters)) {
		return;
	}
	lck_mtx_lock(mbuf_mlock);
	mbuf_drain_locked(ignore_waiters);
	lck_mtx_unlock(mbuf_mlock);
}


static int
m_drain_force_sysctl SYSCTL_HANDLER_ARGS
{
#pragma unused(arg1, arg2)
	int val = 0, err;

	err = sysctl_handle_int(oidp, &val, 0, req);
	if (err != 0 || req->newptr == USER_ADDR_NULL) {
		return err;
	}
	if (val) {
		mbuf_drain(TRUE);
	}

	return err;
}

#if DEBUG || DEVELOPMENT
__printflike(3, 4)
static void
_mbwdog_logger(const char *func, const int line, const char *fmt, ...)
{
	va_list ap;
	struct timeval now;
	char str[384], p[256];
	int len;

	LCK_MTX_ASSERT(mbuf_mlock, LCK_MTX_ASSERT_OWNED);
	if (mbwdog_logging == NULL) {
		/*
		 * This might block under a mutex, which isn't really great,
		 * but this happens once, so we'll live.
		 */
		mbwdog_logging = zalloc_permanent(mbwdog_logging_size,
		    ZALIGN_NONE);
	}
	va_start(ap, fmt);
	vsnprintf(p, sizeof(p), fmt, ap);
	va_end(ap);
	microuptime(&now);
	len = scnprintf(str, sizeof(str),
	    "\n%ld.%d (%d/%llx) %s:%d %s",
	    now.tv_sec, now.tv_usec,
	    proc_getpid(current_proc()),
	    (uint64_t)VM_KERNEL_ADDRPERM(current_thread()),
	    func, line, p);
	if (len < 0) {
		return;
	}
	if (mbwdog_logging_used + len > mbwdog_logging_size) {
		mbwdog_logging_used = mbwdog_logging_used / 2;
		memmove(mbwdog_logging, mbwdog_logging + mbwdog_logging_used,
		    mbwdog_logging_size - mbwdog_logging_used);
		mbwdog_logging[mbwdog_logging_used] = 0;
	}
	strlcat(mbwdog_logging, str, mbwdog_logging_size);
	mbwdog_logging_used += len;
}

#endif // DEBUG || DEVELOPMENT

static void
mtracelarge_register(size_t size)
{
	int i;
	struct mtracelarge *trace;
	uintptr_t bt[MLEAK_STACK_DEPTH];
	unsigned int depth;

	depth = backtrace(bt, MLEAK_STACK_DEPTH, NULL, NULL);
	/* Check if this entry is already on the list. */
	for (i = 0; i < MTRACELARGE_NUM_TRACES; i++) {
		trace = &mtracelarge_table[i];
		if (trace->size == size && trace->depth == depth &&
		    memcmp(bt, trace->addr, depth * sizeof(uintptr_t)) == 0) {
			return;
		}
	}
	for (i = 0; i < MTRACELARGE_NUM_TRACES; i++) {
		trace = &mtracelarge_table[i];
		if (size > trace->size) {
			trace->depth = depth;
			memcpy(trace->addr, bt, depth * sizeof(uintptr_t));
			trace->size = size;
			break;
		}
	}
}

#if DEBUG || DEVELOPMENT

static int
mbuf_wd_dump_sysctl SYSCTL_HANDLER_ARGS
{
	char *str;

	ifnet_head_lock_shared();
	lck_mtx_lock(mbuf_mlock);

	str = mbuf_dump();

	lck_mtx_unlock(mbuf_mlock);
	ifnet_head_done();

	return sysctl_io_string(req, str, 0, 0, NULL);
}

#endif /* DEBUG || DEVELOPMENT */

SYSCTL_DECL(_kern_ipc);
#if DEBUG || DEVELOPMENT
#if SKYWALK
SYSCTL_UINT(_kern_ipc, OID_AUTO, mc_threshold_scale_factor,
    CTLFLAG_RW | CTLFLAG_LOCKED, &mc_threshold_scale_down_factor,
    MC_THRESHOLD_SCALE_DOWN_FACTOR,
    "scale down factor for mbuf cache thresholds");
#endif /* SKYWALK */
SYSCTL_PROC(_kern_ipc, OID_AUTO, mb_wd_dump,
    CTLTYPE_STRING | CTLFLAG_RD | CTLFLAG_LOCKED,
    0, 0, mbuf_wd_dump_sysctl, "A", "mbuf watchdog dump");
#endif /* DEBUG || DEVELOPMENT */
SYSCTL_PROC(_kern_ipc, OID_AUTO, mleak_top_trace,
    CTLTYPE_STRUCT | CTLFLAG_RD | CTLFLAG_LOCKED,
    0, 0, mleak_top_trace_sysctl, "S,mb_top_trace", "");
SYSCTL_PROC(_kern_ipc, OID_AUTO, mleak_table,
    CTLTYPE_STRUCT | CTLFLAG_RD | CTLFLAG_LOCKED,
    0, 0, mleak_table_sysctl, "S,mleak_table", "");
SYSCTL_INT(_kern_ipc, OID_AUTO, mleak_sample_factor,
    CTLFLAG_RW | CTLFLAG_LOCKED, &mleak_table.mleak_sample_factor, 0, "");
SYSCTL_INT(_kern_ipc, OID_AUTO, mb_normalized,
    CTLFLAG_RD | CTLFLAG_LOCKED, &mb_normalized, 0, "");
SYSCTL_INT(_kern_ipc, OID_AUTO, mb_watchdog,
    CTLFLAG_RW | CTLFLAG_LOCKED, &mb_watchdog, 0, "");
SYSCTL_PROC(_kern_ipc, OID_AUTO, mb_drain_force,
    CTLTYPE_INT | CTLFLAG_RW | CTLFLAG_LOCKED, NULL, 0,
    m_drain_force_sysctl, "I",
    "Forces the mbuf garbage collection to run");
SYSCTL_INT(_kern_ipc, OID_AUTO, mb_drain_maxint,
    CTLFLAG_RW | CTLFLAG_LOCKED, &mb_drain_maxint, 0,
    "Minimum time interval between garbage collection");

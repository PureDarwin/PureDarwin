/*! NOTE: Most kern_mbuf functions are implemented by our mbuf.c; however,
 *  a certain few non-allocation-related ones which we can copy wholesale
 *  from FreeBSD live here. */
/*-
 * Copyright (c) 2004, 2005,
 *	Bosko Milekic <bmilekic@FreeBSD.org>.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice unmodified, this list of conditions and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
#include <sys/mbuf.h>

/*
 * Allocate a given length worth of mbufs and/or clusters (whatever fits
 * best) and return a pointer to the top of the allocated chain.  If an
 * existing mbuf chain is provided, then we will append the new chain
 * to the existing one but still return the top of the newly allocated
 * chain.
 */
struct mbuf *
m_getm2(struct mbuf *m, int len, int how, short type, int flags)
{
	struct mbuf *mb, *nm = NULL, *mtail = NULL;

	KASSERT(len >= 0, ("%s: len is < 0", __func__));

	/* Validate flags. */
	flags &= (M_PKTHDR | M_EOR);

	/* Packet header mbuf must be first in chain. */
	if ((flags & M_PKTHDR) && m != NULL)
		flags &= ~M_PKTHDR;

	/* Loop and append maximum sized mbufs to the chain tail. */
	while (len > 0) {
		if (len > MCLBYTES)
			mb = m_getjcl(how, type, (flags & M_PKTHDR),
			    MJUMPAGESIZE);
		else if (len >= MINCLSIZE)
			mb = m_getcl(how, type, (flags & M_PKTHDR));
		else if (flags & M_PKTHDR)
			mb = m_gethdr(how, type);
		else
			mb = m_get(how, type);

		/* Fail the whole operation if one mbuf can't be allocated. */
		if (mb == NULL) {
			if (nm != NULL)
				m_freem(nm);
			return (NULL);
		}

		/* Book keeping. */
		len -= M_SIZE(mb);
		if (mtail != NULL)
			mtail->m_next = mb;
		else
			nm = mb;
		mtail = mb;
		flags &= ~M_PKTHDR;	/* Only valid on the first mbuf. */
	}
	if (flags & M_EOR)
		mtail->m_flags |= M_EOR;  /* Only valid on the last mbuf. */

	/* If mbuf was supplied, append new chain to the end of it. */
	if (m != NULL) {
		for (mtail = m; mtail->m_next != NULL; mtail = mtail->m_next)
			;
		mtail->m_next = nm;
		mtail->m_flags &= ~M_EOR;
	} else
		m = nm;

	return (m);
}

/*-
 * Configure a provided mbuf to refer to the provided external storage
 * buffer and setup a reference count for said buffer.
 *
 * Arguments:
 *    mb     The existing mbuf to which to attach the provided buffer.
 *    buf    The address of the provided external storage buffer.
 *    size   The size of the provided buffer.
 *    freef  A pointer to a routine that is responsible for freeing the
 *           provided external storage buffer.
 *    args   A pointer to an argument structure (of any type) to be passed
 *           to the provided freef routine (may be NULL).
 *    flags  Any other flags to be passed to the provided mbuf.
 *    type   The type that the external storage buffer should be
 *           labeled with.
 *
 * Returns:
 *    Nothing.
 */
void
m_extadd(struct mbuf *mb, caddr_t buf, u_int size,
    void (*freef)(struct mbuf *, void *, void *), void *arg1, void *arg2,
    int flags, int type)
{

	KASSERT(type != EXT_CLUSTER, ("%s: EXT_CLUSTER not allowed", __func__));

	mb->m_flags |= (M_EXT | flags);
	mb->m_ext.ext_buf = buf;
	mb->m_data = mb->m_ext.ext_buf;
	mb->m_ext.ext_size = size;
#ifndef __HAIKU__
	mb->m_ext.ext_free = freef;
	mb->m_ext.ext_arg1 = arg1;
	mb->m_ext.ext_arg2 = arg2;
#else
	if (freef != NULL)
		panic("m_ext.ext_free not yet implemented");
#endif
	mb->m_ext.ext_type = type;

	if (type != EXT_EXTREF) {
		mb->m_ext.ext_count = 1;
		mb->m_ext.ext_flags = EXT_FLAG_EMBREF;
	} else
		mb->m_ext.ext_flags = 0;
}

/*
 * Free an entire chain of mbufs and associated external buffers, if
 * applicable.
 */
void
m_freem(struct mbuf *mb)
{
	while (mb != NULL)
		mb = m_free(mb);
}

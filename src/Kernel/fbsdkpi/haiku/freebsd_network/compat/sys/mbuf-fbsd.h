/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 1982, 1986, 1988, 1993
 *	The Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
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
#ifndef _FBSD_COMPAT_SYS_MBUF_FBSD_H_
#define _FBSD_COMPAT_SYS_MBUF_FBSD_H_

/*
 * Return the address of the start of the buffer associated with an mbuf,
 * handling external storage, packet-header mbufs, and regular data mbufs.
 */
#define	M_START(m)							\
	(((m)->m_flags & M_EXT) ? (m)->m_ext.ext_buf :			\
	 ((m)->m_flags & M_PKTHDR) ? &(m)->m_pktdat[0] :		\
	 &(m)->m_dat[0])

/*
 * Return the size of the buffer associated with an mbuf, handling external
 * storage, packet-header mbufs, and regular data mbufs.
 */
#define	M_SIZE(m)							\
	(((m)->m_flags & M_EXT) ? (m)->m_ext.ext_size :			\
	 ((m)->m_flags & M_PKTHDR) ? MHLEN :				\
	 MLEN)

/*
 * Set the m_data pointer of a newly allocated mbuf to place an object of the
 * specified size at the end of the mbuf, longword aligned.
 *
 * NB: Historically, we had M_ALIGN(), MH_ALIGN(), and MEXT_ALIGN() as
 * separate macros, each asserting that it was called at the proper moment.
 * This required callers to themselves test the storage type and call the
 * right one.  Rather than require callers to be aware of those layout
 * decisions, we centralize here.
 */
static __inline void
m_align(struct mbuf *m, int len)
{
#ifdef INVARIANTS
	const char *msg = "%s: not a virgin mbuf";
#endif
	int adjust;

	KASSERT(m->m_data == M_START(m), (msg, __func__));

	adjust = M_SIZE(m) - len;
	m->m_data += adjust &~ (sizeof(long)-1);
}

#define	M_ALIGN(m, len)		m_align(m, len)
#define	MH_ALIGN(m, len)	m_align(m, len)
#define	MEXT_ALIGN(m, len)	m_align(m, len)

/*
 * Evaluate TRUE if it's safe to write to the mbuf m's data region (this can
 * be both the local data payload, or an external buffer area, depending on
 * whether M_EXT is set).
 */
#define	M_WRITABLE(m)	(!((m)->m_flags & M_RDONLY) &&			\
			 (!(((m)->m_flags & M_EXT)) ||			\
			 (m_extrefcnt(m) == 1)))

/*
 * Compute the amount of space available before the current start of data in
 * an mbuf.
 *
 * The M_WRITABLE() is a temporary, conservative safety measure: the burden
 * of checking writability of the mbuf data area rests solely with the caller.
 *
 * NB: In previous versions, M_LEADINGSPACE() would only check M_WRITABLE()
 * for mbufs with external storage.  We now allow mbuf-embedded data to be
 * read-only as well.
 */
#define	M_LEADINGSPACE(m)						\
	(M_WRITABLE(m) ? ((m)->m_data - M_START(m)) : 0)

/*
 * So M_TRAILINGROOM() is for when you want to know how much space
 * would be there if it was writable. This can be used to
 * detect changes in mbufs by knowing the value at one point
 * and then being able to compare it later to the current M_TRAILINGROOM().
 * The TRAILINGSPACE() macro is not suitable for this since an mbuf
 * at one point might not be writable and then later it becomes writable
 * even though the space at the back of it has not changed.
 */
#define M_TRAILINGROOM(m) ((M_START(m) + M_SIZE(m)) - ((m)->m_data + (m)->m_len))
/*
 * Compute the amount of space available after the end of data in an mbuf.
 *
 * The M_WRITABLE() is a temporary, conservative safety measure: the burden
 * of checking writability of the mbuf data area rests solely with the caller.
 *
 * NB: In previous versions, M_TRAILINGSPACE() would only check M_WRITABLE()
 * for mbufs with external storage.  We now allow mbuf-embedded data to be
 * read-only as well.
 */
#define	M_TRAILINGSPACE(m) (M_WRITABLE(m) ? M_TRAILINGROOM(m) : 0)

/*
 * Arrange to prepend space of size plen to mbuf m.  If a new mbuf must be
 * allocated, how specifies whether to wait.  If the allocation fails, the
 * original mbuf chain is freed and m is set to NULL.
 */
#define	M_PREPEND(m, plen, how) do {					\
	struct mbuf **_mmp = &(m);					\
	struct mbuf *_mm = *_mmp;					\
	int _mplen = (plen);						\
	int __mhow = (how);						\
									\
	MBUF_CHECKSLEEP(how);						\
	if (M_LEADINGSPACE(_mm) >= _mplen) {				\
		_mm->m_data -= _mplen;					\
		_mm->m_len += _mplen;					\
	} else								\
		_mm = m_prepend(_mm, _mplen, __mhow);			\
	if (_mm != NULL && _mm->m_flags & M_PKTHDR)			\
		_mm->m_pkthdr.len += _mplen;				\
	*_mmp = _mm;							\
} while (0)

static __inline void
m_clrprotoflags(struct mbuf *m)
{
	while (m) {
		m->m_flags &= ~M_PROTOFLAGS;
		m = m->m_next;
	}
}

static inline u_int
m_extrefcnt(struct mbuf *m)
{
	KASSERT(m->m_flags & M_EXT, ("%s: M_EXT missing", __func__));

	return ((m->m_ext.ext_flags & EXT_FLAG_EMBREF) ? m->m_ext.ext_count :
		*m->m_ext.ext_cnt);
}

static __inline int
m_gettype(int size)
{
	int type = 0;

	switch (size) {
	case MCLBYTES:
		type = EXT_CLUSTER;
		break;
#if MJUMPAGESIZE != MCLBYTES
	case MJUMPAGESIZE:
		type = EXT_JUMBOP;
		break;
#endif
	case MJUM9BYTES:
		type = EXT_JUMBO9;
		break;
	default:
		panic("%s: invalid cluster size %d", __func__, size);
	}

	return (type);
}

/*
 * XXX: m_cljset() is a dangerous API.  One must attach only a new,
 * unreferenced cluster to an mbuf(9).  It is not possible to assert
 * that, so care can be taken only by users of the API.
 */
static __inline void
m_cljset(struct mbuf *m, void *cl, int type)
{
	int size = 0;

	switch (type) {
	case EXT_CLUSTER:
		size = MCLBYTES;
		break;
#if MJUMPAGESIZE != MCLBYTES
	case EXT_JUMBOP:
		size = MJUMPAGESIZE;
		break;
#endif
	case EXT_JUMBO9:
		size = MJUM9BYTES;
		break;
	default:
		panic("%s: unknown cluster type %d", __func__, type);
		break;
	}

	m->m_data = m->m_ext.ext_buf = (caddr_t)cl;
	m->m_ext.ext_size = size;
	m->m_ext.ext_type = type;
	m->m_ext.ext_flags = EXT_FLAG_EMBREF;
	m->m_ext.ext_count = 1;
	m->m_flags |= M_EXT;
}

/* mbufq */

struct mbufq {
	STAILQ_HEAD(, mbuf)	mq_head;
	int			mq_len;
	int			mq_maxlen;
};

static inline void
mbufq_init(struct mbufq *mq, int maxlen)
{

	STAILQ_INIT(&mq->mq_head);
	mq->mq_maxlen = maxlen;
	mq->mq_len = 0;
}

static inline struct mbuf *
mbufq_flush(struct mbufq *mq)
{
	struct mbuf *m;

	m = STAILQ_FIRST(&mq->mq_head);
	STAILQ_INIT(&mq->mq_head);
	mq->mq_len = 0;
	return (m);
}

static inline void
mbufq_drain(struct mbufq *mq)
{
	struct mbuf *m, *n;

	n = mbufq_flush(mq);
	while ((m = n) != NULL) {
		n = STAILQ_NEXT(m, m_stailqpkt);
		m_freem(m);
	}
}

static inline struct mbuf *
mbufq_first(const struct mbufq *mq)
{

	return (STAILQ_FIRST(&mq->mq_head));
}

static inline struct mbuf *
mbufq_last(const struct mbufq *mq)
{

	return (STAILQ_LAST(&mq->mq_head, mbuf, m_stailqpkt));
}

static inline bool
mbufq_empty(const struct mbufq *mq)
{
	return (mq->mq_len == 0);
}

static inline int
mbufq_full(const struct mbufq *mq)
{

	return (mq->mq_maxlen > 0 && mq->mq_len >= mq->mq_maxlen);
}

static inline int
mbufq_len(const struct mbufq *mq)
{

	return (mq->mq_len);
}

static inline int
mbufq_enqueue(struct mbufq *mq, struct mbuf *m)
{

	if (mbufq_full(mq))
		return (ENOBUFS);
	STAILQ_INSERT_TAIL(&mq->mq_head, m, m_stailqpkt);
	mq->mq_len++;
	return (0);
}

static inline struct mbuf *
mbufq_dequeue(struct mbufq *mq)
{
	struct mbuf *m;

	m = STAILQ_FIRST(&mq->mq_head);
	if (m) {
		STAILQ_REMOVE_HEAD(&mq->mq_head, m_stailqpkt);
		m->m_nextpkt = NULL;
		mq->mq_len--;
	}
	return (m);
}

static inline void
mbufq_prepend(struct mbufq *mq, struct mbuf *m)
{

	STAILQ_INSERT_HEAD(&mq->mq_head, m, m_stailqpkt);
	mq->mq_len++;
}

/*
 * Note: this doesn't enforce the maximum list size for dst.
 */
static inline void
mbufq_concat(struct mbufq *mq_dst, struct mbufq *mq_src)
{

	mq_dst->mq_len += mq_src->mq_len;
	STAILQ_CONCAT(&mq_dst->mq_head, &mq_src->mq_head);
	mq_src->mq_len = 0;
}


#endif	/* _FBSD_COMPAT_SYS_MBUF_FBSD_H_ */

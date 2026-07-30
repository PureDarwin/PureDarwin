/*
 * Copyright (c) 1998-2011 Apple Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.1 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

#ifndef _IOMBUFQUEUE_H
#define _IOMBUFQUEUE_H

extern "C" {
#include <sys/param.h>
#include <sys/mbuf.h>
}

struct IOMbufQueue
{
    mbuf_t      head;
    mbuf_t      tail;
    uint32_t    count;
    uint32_t    capacity;
    uint32_t    bytes;
};

static __inline__
int IOMbufFree(mbuf_t m)
{
	return mbuf_freem_list(m);
}

static __inline__
void IOMbufQueueInit( IOMbufQueue * q, uint32_t capacity = 0 )
{
    q->head  = q->tail = 0;
    q->count = 0;
    q->bytes = 0;
    q->capacity = capacity;
}

static __inline__
bool IOMbufQueueEnqueue( IOMbufQueue * q, mbuf_t m )
{
    if (q->count >= q->capacity)
        return false;

    if (q->count++ > 0)
        mbuf_setnextpkt(q->tail , m);
    else
        q->head = m;

    for (q->tail = m;
         mbuf_nextpkt(q->tail);
         q->tail = mbuf_nextpkt(q->tail), q->count++)
        ;

    return true;
}

static __inline__
bool IOMbufQueueEnqueue( IOMbufQueue * q, IOMbufQueue * qe )
{
    if (qe->count)
    {
        if (q->count == 0)
            q->head = qe->head;
        else
            mbuf_setnextpkt(q->tail , qe->head);
        q->tail = qe->tail;
        q->count += qe->count;

        qe->head = qe->tail = 0;
        qe->count = 0;
    }
    return true;
}

static __inline__
void IOMbufQueuePrepend( IOMbufQueue * q, mbuf_t m )
{
    mbuf_t tail;

    for (tail = m, q->count++;
         mbuf_nextpkt(tail);
         tail = mbuf_nextpkt(tail), q->count++)
        ;

    mbuf_setnextpkt(tail , q->head);
    if (q->tail == 0)
        q->tail = tail;
    q->head = m;
}

static __inline__
void IOMbufQueuePrepend( IOMbufQueue * q, IOMbufQueue * qp )
{
    if (qp->count)
    {
        mbuf_setnextpkt(qp->tail , q->head);
        if (q->tail == 0)
            q->tail = qp->tail;
        q->head = qp->head;
        q->count += qp->count;

        qp->head = qp->tail = 0;
        qp->count = 0;
    }
}

static __inline__
mbuf_t IOMbufQueueDequeue( IOMbufQueue * q )
{   
    mbuf_t m = q->head;
    if (m)
    {
        if ((q->head = mbuf_nextpkt(m)) == 0)
            q->tail = 0;
        mbuf_setnextpkt(m , 0);
        q->count--;
    }
    return m;
}

static __inline__
mbuf_t IOMbufQueueDequeueAll( IOMbufQueue * q )
{
    mbuf_t m = q->head;
    q->head = q->tail = 0;
    q->count = 0;
    return m;
}

static __inline__
mbuf_t IOMbufQueuePeek( IOMbufQueue * q )
{
    return q->head;
}

static __inline__
uint32_t IOMbufQueueGetSize( IOMbufQueue * q )
{
    return q->count;
}

static __inline__
uint32_t IOMbufQueueIsEmpty( IOMbufQueue * q )
{
    return (0 == q->count);
}

static __inline__
uint32_t IOMbufQueueGetCapacity( IOMbufQueue * q )
{
    return q->capacity;
}

static __inline__
void IOMbufQueueSetCapacity( IOMbufQueue * q, uint32_t capacity )
{
	q->capacity = capacity;
}

static __inline__
void IOMbufQueueTailAdd( IOMbufQueue * q, mbuf_t m, uint32_t len )
{
    if (q->count == 0)
    {
        q->head = q->tail = m;
    }
    else
    {
        mbuf_setnextpkt(q->tail, m);
        q->tail = m;
    }
    q->count++;
    q->bytes += len;
}

#endif /* !_IOMBUFQUEUE_H */

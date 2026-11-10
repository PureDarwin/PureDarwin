/* Copyright (c) 2014-2015, 2018, The Linux Foundation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 *       copyright notice, this list of conditions and the following
 *       disclaimer in the documentation and/or other materials provided
 *       with the distribution.
 *     * Neither the name of The Linux Fundation, Inc. nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

// PDQcomSMD.cpp - qualcomm smd stream channels over smem, from lk2nd platform/msm_shared/smd.c
// SPDX-License-Identifier: BSD-3-Clause

#include <IOKit/IOLib.h>
#include <string.h>

#include "PDQcomSMD.h"
#include "PDQcomSMEM.h"

#define SMD_SS_CLOSED		0x0
#define SMD_SS_OPENING		0x1
#define SMD_SS_OPENED		0x2
#define SMD_SS_FLUSHING		0x3
#define SMD_SS_CLOSING		0x4

#define SMEM_CHANNEL_ALLOC_TBL	13
#define SMEM_SMD_BASE_ID	14
#define SMEM_SMD_FIFO_BASE_ID	338
#define SMD_NUM_STREAM_CHANNELS	64
#define SMD_PKT_MAX		8192

// the rpm edge uses lk2nd's word-wide port, the pronto partition a 20-byte side with byte flags
// source: openstick smem-before.bin of pd-prontoboot-pmu-transition and the prontoboot-no-pas-smem-film port film
enum {
	kState, kDsr, kCts, kCd, kRi, kWritten, kRead, kUpdated, kMask, kTail, kHead, kFields
};

static const uint8_t wordLayout[kFields] = { 0, 4, 8, 12, 16, 20, 24, 28, 32, 36, 40 };
static const uint8_t byteLayout[kFields] = { 0, 4, 5, 6, 7, 8, 9, 10, 11, 12, 16 };

struct PDQcomSMDPort {
	volatile uint8_t *side[2];
	const uint8_t *layout;
	bool bytes;

	uint32_t get(int s, int f)
	{
		volatile uint8_t *p = side[s] + layout[f];
		return bytes && f >= kDsr && f <= kMask ? *p : *(volatile uint32_t *)p;
	}

	void set(int s, int f, uint32_t v)
	{
		volatile uint8_t *p = side[s] + layout[f];
		if (bytes && f >= kDsr && f <= kMask)
			*p = (uint8_t)v;
		else
			*(volatile uint32_t *)p = v;
	}
};

struct smd_channel_alloc_entry {
	char name[20];
	uint32_t cid;
	uint32_t ctype;
	uint32_t ref_count;
};

struct smd_pkt_hdr {
	uint32_t pkt_size;
	uint32_t app_field;
	uint32_t app_ptr;
	uint32_t kind;
	uint32_t priority;
};

PDQcomSMDChannel *
PDQcomSMDChannel::open(const char *name, uint32_t edge, int host, volatile uint32_t *ipc, uint32_t ipcBit,
    PDQcomSMDRecv recv, void *ctx)
{
	uint32_t size = 0;
	struct smd_channel_alloc_entry *tbl = (struct smd_channel_alloc_entry *)PDQcomSMEM::get(host,
	    SMEM_CHANNEL_ALLOC_TBL, &size);
	int i;

	if (tbl == NULL)
		return NULL;
	for (i = 0; i < SMD_NUM_STREAM_CHANNELS && (i + 1) * sizeof(*tbl) <= size; i++)
		if ((tbl[i].ctype & 0xff) == edge && strncmp(tbl[i].name, name, sizeof(tbl[i].name)) == 0)
			break;
	if (i == SMD_NUM_STREAM_CHANNELS || (i + 1) * sizeof(*tbl) > size)
		return NULL;

	uint32_t cid = tbl[i].cid, portSize = 0, fifoSize = 0;
	uint8_t *raw = (uint8_t *)PDQcomSMEM::get(host, SMEM_SMD_BASE_ID + cid, &portSize);
	uint8_t *fifo = (uint8_t *)PDQcomSMEM::get(host, SMEM_SMD_FIFO_BASE_ID + cid, &fifoSize);
	if (raw == NULL || fifo == NULL || (portSize != 40 && portSize < 88) || fifoSize < 64)
		return NULL;

	PDQcomSMDPort *port = new PDQcomSMDPort;
	port->bytes = portSize == 40;
	port->layout = port->bytes ? byteLayout : wordLayout;
	port->side[0] = raw;
	port->side[1] = raw + (port->bytes ? 20 : 44);

	PDQcomSMDChannel *ch = new PDQcomSMDChannel;
	strlcpy(ch->fName, name, sizeof(ch->fName));
	ch->fPort = port;
	ch->fFifoSize = fifoSize / 2;
	ch->fTx = fifo;
	ch->fRx = fifo + ch->fFifoSize;
	ch->fIpc = ipc;
	ch->fIpcBit = ipcBit;
	ch->fRecv = recv;
	ch->fCtx = ctx;
	ch->fState = SMD_SS_CLOSED;
	ch->fPkt = (uint8_t *)IOMallocData(SMD_PKT_MAX);
	ch->fPktSize = 0;
	IOLog("PDQcomSMD: %s cid %u fifo %u, remote state %u\n", name, cid, ch->fFifoSize, port->get(1, kState));

	ch->setState(SMD_SS_OPENING);
	ch->kick();
	return ch;
}

void
PDQcomSMDChannel::kick(void)
{
	__asm__ volatile ("dsb sy" ::: "memory");
	*fIpc = 1u << fIpcBit;
}

void
PDQcomSMDChannel::setState(uint32_t state)
{
	uint32_t cur = fPort->get(0, kState);

	switch (state) {
	case SMD_SS_CLOSED:
		state = cur == SMD_SS_OPENED ? SMD_SS_CLOSING : SMD_SS_CLOSED;
		break;
	case SMD_SS_OPENING:
		if (cur != SMD_SS_CLOSING && cur != SMD_SS_CLOSED)
			return;
		fPort->set(1, kTail, 0);
		fPort->set(0, kHead, 0);
		if (!fPort->bytes)
			fPort->set(0, kMask, 0);
		break;
	case SMD_SS_OPENED:
		if (cur != SMD_SS_OPENING)
			return;
		break;
	case SMD_SS_CLOSING:
		if (cur != SMD_SS_OPENED)
			return;
		break;
	default:
		return;
	}

	uint32_t on = state == SMD_SS_OPENED ? 1 : 0;
	fPort->set(0, kDsr, on);
	fPort->set(0, kCts, on);
	fPort->set(0, kCd, on);
	fPort->set(0, kState, state);
	fPort->set(0, kUpdated, 1);
	fState = state;
}

bool
PDQcomSMDChannel::isOpen(void)
{
	uint32_t remote = fPort->get(1, kState);

	return fPort->get(0, kState) == SMD_SS_OPENED && (remote == SMD_SS_OPENED || remote == SMD_SS_FLUSHING);
}

uint32_t
PDQcomSMDChannel::avail(void)
{
	uint32_t w = fPort->get(1, kHead), r = fPort->get(1, kTail);

	return w >= r ? w - r : fFifoSize - r + w;
}

// the rpm's fifos sit in its message ram, which takes 32-bit accesses only, as lk2nd's writel/readl copies do
void
PDQcomSMDChannel::readFifo(uint8_t *dst, uint32_t len)
{
	uint32_t ri = fPort->get(1, kTail);

	if (!fPort->bytes) {
		// every byte through the aligned word that holds it, whatever the packet length
		for (; len != 0; len--) {
			uint32_t w = *(volatile uint32_t *)(fRx + (ri & ~3u));
			*dst++ = (uint8_t)(w >> (8 * (ri & 3)));
			if (++ri >= fFifoSize)
				ri = 0;
		}
		__asm__ volatile ("dmb sy" ::: "memory");
		fPort->set(1, kTail, ri);
		return;
	}
	while (len--) {
		*dst++ = fRx[ri];
		if (++ri >= fFifoSize)
			ri = 0;
	}
	__asm__ volatile ("dmb sy" ::: "memory");
	fPort->set(1, kTail, ri);
}

void
PDQcomSMDChannel::writeFifo(const uint8_t *src, uint32_t len)
{
	uint32_t wi = fPort->get(0, kHead);

	if (!fPort->bytes && ((wi | len) & 3) == 0) {
		for (; len != 0; len -= 4, src += 4) {
			uint32_t w;
			memcpy(&w, src, 4);
			*(volatile uint32_t *)(fTx + wi) = w;
			if ((wi += 4) >= fFifoSize)
				wi = 0;
		}
		__asm__ volatile ("dmb sy" ::: "memory");
		fPort->set(0, kHead, wi);
		return;
	}
	while (len--) {
		fTx[wi] = *src++;
		if (++wi >= fFifoSize)
			wi = 0;
	}
	__asm__ volatile ("dmb sy" ::: "memory");
	fPort->set(0, kHead, wi);
}

// state changes and inbound packets, called from the owner's poll loop or interrupt
void
PDQcomSMDChannel::poll(void)
{
	bool notify = false;

	if (fPort->get(1, kUpdated)) {
		fPort->set(1, kUpdated, 0);
		notify = true;
	}
	uint32_t remote = fPort->get(1, kState);
	if (fState != remote && (remote == SMD_SS_OPENING || remote == SMD_SS_OPENED) && fState == SMD_SS_OPENING) {
		setState(SMD_SS_OPENED);
		IOLog("PDQcomSMD: %s open\n", fName);
		notify = true;
	} else if (remote == SMD_SS_CLOSING && fState == SMD_SS_OPENED) {
		setState(SMD_SS_CLOSED);
		IOLog("PDQcomSMD: %s closed by remote\n", fName);
		notify = true;
	}

	while (fState == SMD_SS_OPENED) {
		if (fPktSize == 0) {
			struct smd_pkt_hdr hdr;
			if (avail() < sizeof(hdr))
				break;
			readFifo((uint8_t *)&hdr, sizeof(hdr));
			fPktSize = hdr.pkt_size ? hdr.pkt_size : 1;
			if (hdr.pkt_size == 0 || hdr.pkt_size > SMD_PKT_MAX) {
				IOLog("PDQcomSMD: %s bad packet size %u\n", fName, hdr.pkt_size);
				fPktSize = 0;
				continue;
			}
		}
		if (avail() < fPktSize)
			break;
		uint32_t len = fPktSize;
		readFifo(fPkt, len);
		fPktSize = 0;
		fPort->set(1, kWritten, 0);
		fPort->set(0, kRead, 1);
		notify = true;
		if (fRecv != NULL)
			fRecv(fCtx, fPkt, len);
	}
	if (notify)
		kick();
}

int
PDQcomSMDChannel::send(const void *data, uint32_t len)
{
	struct smd_pkt_hdr hdr;

	if (!isOpen() || !fPort->get(0, kDsr))
		return -1;

	uint32_t w = fPort->get(0, kHead), r = fPort->get(0, kTail);
	uint32_t space = fFifoSize - 1 - (w >= r ? w - r : fFifoSize - r + w);
	if (len + sizeof(hdr) > space)
		return -2;

	memset(&hdr, 0, sizeof(hdr));
	hdr.pkt_size = len;
	fPort->set(1, kRead, 0);
	writeFifo((const uint8_t *)&hdr, sizeof(hdr));
	writeFifo((const uint8_t *)data, len);
	fPort->set(0, kWritten, 1);
	// the pronto film keeps the apps side's last flag byte at one throughout
	if (!fPort->bytes)
		fPort->set(0, kMask, 0);
	kick();
	return 0;
}

void
PDQcomSMDChannel::close(void)
{
	setState(SMD_SS_CLOSED);
	kick();
	IOFreeData(fPkt, SMD_PKT_MAX);
	delete fPort;
	delete this;
}

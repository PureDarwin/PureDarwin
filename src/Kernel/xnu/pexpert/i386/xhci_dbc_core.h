// xhci_dbc_core.h - xHCI Debug Capability console core, shared by the EFI loader and the xnu kernel
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause
/*
 * Ported from the DbC work of Hiroki Sato for FreeBSD (xhci_dbc.c and the
 * loader's xhci_dbc_cons.c, branch stable-14-hrs-usbdbc), reworked into one
 * freestanding header: register layout, context setup, event ring handling
 * and the polled bulk transfers follow that code.
 *
 * Copyright (c) 2019-2024 Hiroki Sato <hrs@FreeBSD.org>
 * Copyright (c) 2026 The PureDarwin Project
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
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
#ifndef XHCI_DBC_CORE_H
#define XHCI_DBC_CORE_H

#include <stdint.h>
#include <stddef.h>

/*
 * The includer provides, before this header:
 *   DBC_UDELAY(us)   busy wait
 *   DBC_IRQ_SAVE()   returns a cookie, interrupts off
 *   DBC_IRQ_RESTORE(cookie)
 * All DbC state lives in one physically contiguous block that the loader
 * allocates and the kernel keeps using. Only physical addresses are stored in
 * it: every entry point is handed the block's and the registers' virtual
 * addresses (struct dbc), because the kernel's mapping moves during boot.
 */

#define DBC_MAGIC       0x43424450u     /* "PDBC" */
#define DBC_VERSION     1u

/* layout of the block, offsets from its start */
#define DBC_OFF_CTX     0x1000u
#define DBC_OFF_ERST    0x2000u
#define DBC_OFF_STR     0x3000u
#define DBC_OFF_ER      0x4000u
#define DBC_OFF_OR      0x5000u
#define DBC_OFF_IR      0x6000u
#define DBC_OFF_OBUF    0x8000u         /* 32 KiB, never crosses a 64 KiB boundary */
#define DBC_OFF_IBUF    0x10000u
#define DBC_BLOCK_SIZE  0x11000u
#define DBC_BLOCK_ALIGN 0x10000u

#define DBC_RING_TRBS   256u            /* one page, the last entry is a link */
#define DBC_RING_LAST   (DBC_RING_TRBS - 1)
#define DBC_OBUF_SIZE   0x8000u
#define DBC_IBUF_SIZE   0x1000u
#define DBC_IN_TD       1024u           /* one max packet */

/* DbC registers, offsets from the extended capability */
#define DC_DB           0x04
#define DC_ERSTSZ       0x08
#define DC_ERSTBA       0x10
#define DC_ERDP         0x18
#define DC_CTRL         0x20
#define DC_PORTSC       0x28
#define DC_CP           0x30
#define DC_DDI1         0x38
#define DC_DDI2         0x3c

#define DC_CTRL_DCR     0x00000001u
#define DC_CTRL_DRC     0x00000010u     /* write 1 to clear */
#define DC_CTRL_DCE     0x80000000u
#define DC_CTRL_MBS(x)  (((x) >> 16) & 0xff)

#define DC_PORTSC_CCS   0x00000001u
#define DC_PORTSC_CHG   0x00e20000u     /* CSC PRC PLC CEC, write 1 to clear */

#define DC_DB_OUT       0x0000u
#define DC_DB_IN        0x0100u

#define DBC_XECP_ID     0x0a

#define DC_VENDOR       0x1d6b
#define DC_PRODUCT      0x0011
#define DC_REVISION     0x0010
#define DC_PROTOCOL     0x0000

#define DC_STR_MANUFACTURER "The PureDarwin Project"
#define DC_STR_PRODUCT      "PureDarwin USB DbC.GP Device"
#define DC_STR_SERIAL       "0001"

/* TRB types and completion codes (xHCI 6.4) */
#define TRB_NORMAL      1u
#define TRB_LINK        6u
#define TRB_EV_XFER     32u
#define TRB_EV_PORT     34u
#define TRB_CYCLE       0x01u
#define TRB_TC          0x02u
#define TRB_ISP         0x04u
#define TRB_IOC         0x20u
#define CC_SUCCESS      1u
#define CC_SHORT        13u

struct dbc_trb {
	uint64_t ptr;
	uint32_t status;
	uint32_t ctrl;
};

struct dbc_ep_ctx {
	uint32_t dw0;
	uint32_t dw1;
	uint64_t deq;
	uint32_t dw4;
	uint32_t pad[11];
};

/* DbC context, three 64-byte contexts: info, bulk OUT, bulk IN */
struct dbc_ctx {
	uint32_t info[16];
	struct dbc_ep_ctx out;
	struct dbc_ep_ctx in;
};

struct dbc_erst {
	uint64_t base;
	uint32_t size;
	uint32_t rsvd;
	uint32_t pad[12];
};

struct dbc_state {
	uint32_t magic;
	uint32_t version;
	uint64_t base;          /* cpu physical address of the block */
	uint64_t bus;           /* address the controller uses for it */
	uint64_t size;
	uint64_t mmio;          /* physical address of the xHCI registers */
	uint64_t mmio_len;
	uint32_t dbc_off;       /* DbC registers, from mmio */
	uint32_t rid;           /* PCI bus << 8 | device << 3 | function */
	uint32_t lock;
	uint32_t suspended;     /* the host controller is being reset */
	uint32_t was_cfg;       /* a host has configured the device before */
	uint32_t cfg;           /* last seen DCR */
	uint32_t stalled;       /* a flush timed out, do not wait again */
	uint32_t er_deq, er_cyc;
	uint32_t o_enq, o_cyc, o_busy;
	uint32_t o_wenq, o_wdeq;        /* free running */
	uint32_t i_enq, i_cyc, i_busy;
	uint32_t i_wenq, i_wdeq;
	uint32_t n_events, n_err, n_drop, n_reinit, n_poll, n_idle;
};

struct dbc {
	struct dbc_state *s;
	volatile uint8_t *regs;         /* DbC registers */
	uint8_t *mem;                   /* the block */
};

#define DBC_FENCE()     __asm__ volatile("mfence" ::: "memory")

#ifdef DBC_REG_HOOK
/* host side model of a controller, for tests only */
uint32_t dbc_hook_rd(struct dbc *d, uint32_t off);
void dbc_hook_wr(struct dbc *d, uint32_t off, uint32_t v);
#define DBC_RD(d, off)  dbc_hook_rd(d, off)
#define DBC_WR(d, off, v) dbc_hook_wr(d, off, v)
#else
#define DBC_RD(d, off)  (*(volatile uint32_t *)((d)->regs + (off)))
#define DBC_WR(d, off, v) (*(volatile uint32_t *)((d)->regs + (off)) = (v))
#endif

static inline uint32_t dbc_rd(struct dbc *d, uint32_t off)
{
	return DBC_RD(d, off);
}

static inline void dbc_wr(struct dbc *d, uint32_t off, uint32_t v)
{
	DBC_WR(d, off, v);
}

static inline void dbc_wr64(struct dbc *d, uint32_t off, uint64_t v)
{
	dbc_wr(d, off, (uint32_t)v);
	dbc_wr(d, off + 4, (uint32_t)(v >> 32));
}

static inline uint64_t dbc_rd64(struct dbc *d, uint32_t off)
{
	return (uint64_t)dbc_rd(d, off) | ((uint64_t)dbc_rd(d, off + 4) << 32);
}

static inline void dbc_zero(volatile void *p, size_t n)
{
	volatile uint8_t *b = (volatile uint8_t *)p;
	while (n--)
		*b++ = 0;
}

static inline struct dbc_trb *dbc_ring(struct dbc *d, uint32_t off)
{
	return (struct dbc_trb *)(d->mem + off);
}

static inline uint64_t dbc_bus(struct dbc *d, uint32_t off)
{
	return d->s->bus + off;
}

/* a controller that is gone or powered down reads all ones */
static inline int dbc_alive(struct dbc *d)
{
	return dbc_rd(d, DC_CTRL) != 0xffffffffu;
}

static inline void dbc_trb_put(struct dbc_trb *t, uint64_t ptr, uint32_t status, uint32_t ctrl)
{
	volatile struct dbc_trb *v = t;
	v->ptr = ptr;
	v->status = status;
	DBC_FENCE();
	v->ctrl = ctrl;         /* the cycle bit hands the TRB to the controller, last */
	DBC_FENCE();
}

/* a producer ring: zeroed, with the link TRB at the tail */
static inline void dbc_ring_reset(struct dbc *d, uint32_t off)
{
	struct dbc_trb *r = dbc_ring(d, off);

	dbc_zero(r, DBC_RING_TRBS * sizeof(*r));
	r[DBC_RING_LAST].ptr = dbc_bus(d, off);
	r[DBC_RING_LAST].ctrl = (TRB_LINK << 10) | TRB_TC;
}

static inline void dbc_str_put(uint8_t *p, const char *s, uint32_t *lenp)
{
	uint32_t i = 0;

	while (s[i] && i < 30) {
		p[2 + i * 2] = (uint8_t)s[i];
		p[3 + i * 2] = 0;
		i++;
	}
	p[0] = (uint8_t)(2 + i * 2);
	p[1] = 3;
	*lenp = 2 + i * 2;
}

static inline void dbc_ep_init(struct dbc_ep_ctx *e, uint32_t mbs, uint32_t type, uint64_t ring)
{
	dbc_zero(e, sizeof(*e));
	e->dw1 = (type << 3) | (mbs << 8) | (1024u << 16);
	e->deq = ring | 1;                      /* dequeue cycle state 1 */
	e->dw4 = 3 * 1024;                      /* average TRB length */
}

/* Disable the DbC and wait for it to go quiet. */
static inline int dbc_disable(struct dbc *d)
{
	uint32_t i;

	dbc_wr(d, DC_CTRL, dbc_rd(d, DC_CTRL) & ~DC_CTRL_DCE);
	for (i = 0; i < 1000; i++) {
		if (!(dbc_rd(d, DC_CTRL) & (DC_CTRL_DCE | DC_CTRL_DCR)))
			return 1;
		DBC_UDELAY(100);
	}
	return 0;
}

/*
 * Build the context and rings, program the registers and enable the DbC
 * (xHCI 7.6.4.1). Used by the loader and again whenever the controller
 * lost the registers, such as after a host controller reset.
 */
static inline int dbc_setup(struct dbc *d)
{
	struct dbc_state *s = d->s;
	struct dbc_ctx *c = (struct dbc_ctx *)(d->mem + DBC_OFF_CTX);
	struct dbc_erst *e = (struct dbc_erst *)(d->mem + DBC_OFF_ERST);
	uint8_t *str = d->mem + DBC_OFF_STR;
	uint32_t l[4], mbs, i;

	dbc_disable(d);

	dbc_zero(d->mem + DBC_OFF_ER, DBC_RING_TRBS * sizeof(struct dbc_trb));
	dbc_ring_reset(d, DBC_OFF_OR);
	dbc_ring_reset(d, DBC_OFF_IR);
	s->er_deq = 0;
	s->er_cyc = 1;
	s->o_enq = s->i_enq = 0;
	s->o_cyc = s->i_cyc = 1;
	s->o_busy = s->i_busy = 0;
	/* keep the unsent output and drop what was received but never read */
	s->i_wenq = s->i_wdeq = 0;
	s->cfg = 0;

	dbc_zero(str, 4 * 64);
	str[0] = 4;
	str[1] = 3;
	str[2] = 0x09;
	str[3] = 0x04;                          /* language id 0x0409 */
	l[0] = 4;
	dbc_str_put(str + 64, DC_STR_MANUFACTURER, &l[1]);
	dbc_str_put(str + 128, DC_STR_PRODUCT, &l[2]);
	dbc_str_put(str + 192, DC_STR_SERIAL, &l[3]);

	dbc_zero(c, sizeof(*c));
	for (i = 0; i < 4; i++) {
		uint64_t a = dbc_bus(d, DBC_OFF_STR) + i * 64;
		c->info[i * 2] = (uint32_t)a;
		c->info[i * 2 + 1] = (uint32_t)(a >> 32);
	}
	c->info[8] = l[0] | (l[1] << 8) | (l[2] << 16) | (l[3] << 24);
	mbs = DC_CTRL_MBS(dbc_rd(d, DC_CTRL));
	dbc_ep_init(&c->out, mbs, 2, dbc_bus(d, DBC_OFF_OR));   /* bulk OUT */
	dbc_ep_init(&c->in, mbs, 6, dbc_bus(d, DBC_OFF_IR));    /* bulk IN */

	dbc_zero(e, sizeof(*e));
	e->base = dbc_bus(d, DBC_OFF_ER);
	e->size = DBC_RING_TRBS;
	DBC_FENCE();

	dbc_wr(d, DC_ERSTSZ, 1);
	dbc_wr64(d, DC_ERSTBA, dbc_bus(d, DBC_OFF_ERST));
	dbc_wr64(d, DC_ERDP, dbc_bus(d, DBC_OFF_ER));
	dbc_wr64(d, DC_CP, dbc_bus(d, DBC_OFF_CTX));
	dbc_wr(d, DC_DDI1, ((uint32_t)DC_VENDOR << 16) | DC_PROTOCOL);
	dbc_wr(d, DC_DDI2, ((uint32_t)DC_REVISION << 16) | DC_PRODUCT);
	DBC_FENCE();

	dbc_wr(d, DC_CTRL, dbc_rd(d, DC_CTRL) | DC_CTRL_DCE);
	for (i = 0; i < 100; i++) {
		if (dbc_rd(d, DC_CTRL) & DC_CTRL_DCE)
			return 1;
		DBC_UDELAY(100);
	}
	return 0;
}

/* Find the DbC extended capability; returns its byte offset from the registers, or 0. */
static inline uint32_t dbc_find_xecp(volatile uint8_t *mmio, uint64_t len)
{
	uint32_t hcc = *(volatile uint32_t *)(mmio + 0x10);
	uint32_t off = (hcc >> 16) << 2;
	int guard;

	for (guard = 0; guard < 64 && off != 0 && (uint64_t)off + 0x40 <= len; guard++) {
		uint32_t cap = *(volatile uint32_t *)(mmio + off);

		if ((cap & 0xff) == DBC_XECP_ID)
			return off;
		if (((cap >> 8) & 0xff) == 0)
			break;
		off += ((cap >> 8) & 0xff) << 2;
	}
	return 0;
}

/* Handle a controller that forgot the DbC: rebuild it from the block. */
static inline void dbc_recover(struct dbc *d)
{
	d->s->n_reinit++;
	dbc_setup(d);
}

/* Resync a ring with the dequeue pointer the controller keeps in the context. */
static inline void dbc_resync_ep(struct dbc *d, int out)
{
	struct dbc_state *s = d->s;
	struct dbc_ctx *c = (struct dbc_ctx *)(d->mem + DBC_OFF_CTX);
	uint64_t dq = out ? c->out.deq : c->in.deq;
	uint64_t base = dbc_bus(d, out ? DBC_OFF_OR : DBC_OFF_IR);
	uint32_t idx = (uint32_t)((dq & ~0xfull) - base) / (uint32_t)sizeof(struct dbc_trb);
	uint32_t cyc = (uint32_t)(dq & 1);

	if (idx >= DBC_RING_LAST) {
		/* parked on the link TRB: the next lap starts with the other cycle state */
		idx = 0;
		cyc ^= 1;
	}
	if (out) {
		s->o_enq = idx;
		s->o_cyc = cyc;
		s->o_busy = 0;
	} else {
		s->i_enq = idx;
		s->i_cyc = cyc;
		s->i_busy = 0;
	}
}

static inline void dbc_event(struct dbc *d, struct dbc_trb *t)
{
	struct dbc_state *s = d->s;
	uint32_t type = (t->ctrl >> 10) & 0x3f;
	uint32_t cc = t->status >> 24;
	uint32_t resid = t->status & 0xffffff;
	uint32_t ep = (t->ctrl >> 16) & 0x1f;
	uint32_t ok = (cc == CC_SUCCESS || cc == CC_SHORT);

	s->n_events++;
	if (type == TRB_EV_PORT) {
		dbc_wr(d, DC_PORTSC, dbc_rd(d, DC_PORTSC) & DC_PORTSC_CHG);
		return;
	}
	if (type != TRB_EV_XFER)
		return;
	if (ep == 0 || ep == 2) {                       /* bulk OUT */
		if (!s->o_busy)
			return;
		if (ok) {
			s->o_wdeq += s->o_busy - resid;
			s->o_busy = 0;
			s->stalled = 0;
		} else {
			s->n_err++;
			dbc_resync_ep(d, 1);
		}
	} else if (ep == 1 || ep == 3) {                /* bulk IN */
		if (!s->i_busy)
			return;
		if (ok) {
			s->i_wenq += DBC_IN_TD - resid;
			s->i_busy = 0;
		} else {
			s->n_err++;
			dbc_resync_ep(d, 0);
		}
	}
}

/* Consume the event ring. Memory only until there is an event. */
static inline void dbc_events(struct dbc *d)
{
	struct dbc_state *s = d->s;
	struct dbc_trb *er = dbc_ring(d, DBC_OFF_ER);
	int any = 0;

	for (;;) {
		struct dbc_trb *t = &er[s->er_deq];
		struct dbc_trb ev;

		DBC_FENCE();
		if ((((volatile struct dbc_trb *)t)->ctrl & TRB_CYCLE) != s->er_cyc)
			break;
		ev.ptr = ((volatile struct dbc_trb *)t)->ptr;
		ev.status = ((volatile struct dbc_trb *)t)->status;
		ev.ctrl = ((volatile struct dbc_trb *)t)->ctrl;
		dbc_event(d, &ev);
		if (s->er_deq == DBC_RING_LAST)
			s->er_cyc ^= 1;
		s->er_deq = (s->er_deq + 1) & (DBC_RING_TRBS - 1);
		any = 1;
	}
	if (any)
		dbc_wr64(d, DC_ERDP, dbc_bus(d, DBC_OFF_ER) + (uint64_t)s->er_deq * sizeof(struct dbc_trb));
}

/* Place one TD on a producer ring and ring its doorbell. */
static inline void dbc_push(struct dbc *d, int out, uint64_t addr, uint32_t len)
{
	struct dbc_state *s = d->s;
	uint32_t *enq = out ? &s->o_enq : &s->i_enq;
	uint32_t *cyc = out ? &s->o_cyc : &s->i_cyc;
	struct dbc_trb *r = dbc_ring(d, out ? DBC_OFF_OR : DBC_OFF_IR);

	dbc_trb_put(&r[*enq], addr, len,
	    (TRB_NORMAL << 10) | TRB_IOC | (out ? 0 : TRB_ISP) | *cyc);
	*enq += 1;
	if (*enq == DBC_RING_LAST) {
		/* hand over the link TRB and wrap, the controller toggles its cycle state */
		r[DBC_RING_LAST].ctrl = (TRB_LINK << 10) | TRB_TC | *cyc;
		DBC_FENCE();
		*enq = 0;
		*cyc ^= 1;
	}
	if (out)
		s->o_busy = len;
	else
		s->i_busy = len;
	dbc_wr(d, DC_DB, out ? DC_DB_OUT : DC_DB_IN);
}

/* Look at the link state and start any transfer that is due. */
static inline void dbc_kick(struct dbc *d)
{
	struct dbc_state *s = d->s;
	uint32_t ctrl = dbc_rd(d, DC_CTRL);

	if (ctrl == 0xffffffffu)
		return;
	if (!(ctrl & DC_CTRL_DCE)) {
		/* the controller forgot us (reset, resume) */
		dbc_recover(d);
		return;
	}
	if (dbc_rd(d, DC_PORTSC) & DC_PORTSC_CHG)
		dbc_wr(d, DC_PORTSC, dbc_rd(d, DC_PORTSC) & DC_PORTSC_CHG);
	if (ctrl & DC_CTRL_DRC) {
		/* DCR changed, the doorbell stays dead until this is cleared */
		dbc_wr(d, DC_CTRL, ctrl);
		if (!(ctrl & DC_CTRL_DCR) && s->was_cfg) {
			/* the host went away: restart from a clean context */
			s->was_cfg = 0;
			s->n_reinit++;
			dbc_setup(d);
			return;
		}
	}
	s->cfg = ctrl & DC_CTRL_DCR;
	if (!s->cfg) {
		s->o_busy = s->i_busy = 0;
		return;
	}
	s->was_cfg = 1;

	if (!s->o_busy && s->o_wenq != s->o_wdeq) {
		uint32_t used = s->o_wenq - s->o_wdeq;
		uint32_t pos = s->o_wdeq & (DBC_OBUF_SIZE - 1);
		uint32_t len = DBC_OBUF_SIZE - pos;

		if (len > used)
			len = used;
		dbc_push(d, 1, dbc_bus(d, DBC_OFF_OBUF) + pos, len);
	}
	if (!s->i_busy) {
		if (s->i_wenq == s->i_wdeq)
			s->i_wenq = s->i_wdeq = 0;
		if (s->i_wenq == 0)
			dbc_push(d, 0, dbc_bus(d, DBC_OFF_IBUF), DBC_IN_TD);
	}
}

static inline void dbc_poll(struct dbc *d)
{
	d->s->n_poll++;
	dbc_events(d);
	dbc_kick(d);
}

/* Wait, bounded, for the output ring to empty. Gives up for good if the host does not read. */
static inline void dbc_drain(struct dbc *d, uint32_t spins)
{
	struct dbc_state *s = d->s;

	while (spins--) {
		dbc_poll(d);
		if (s->o_wenq == s->o_wdeq && !s->o_busy)
			return;
		if (!s->cfg)
			return;         /* no host yet, the output waits in the ring */
	}
	s->stalled = 1;
}

static inline void dbc_lock(struct dbc *d, uint32_t *flags)
{
	uint32_t n = 100000;

	*flags = (uint32_t)DBC_IRQ_SAVE();
	while (__sync_lock_test_and_set(&d->s->lock, 1) && --n)
		;
}

static inline void dbc_unlock(struct dbc *d, uint32_t flags)
{
	__sync_lock_release(&d->s->lock);
	DBC_IRQ_RESTORE(flags);
}

static inline void dbc_putc_locked(struct dbc *d, uint8_t c)
{
	struct dbc_state *s = d->s;
	uint32_t used = s->o_wenq - s->o_wdeq;

	if (used >= DBC_OBUF_SIZE) {
		if (!s->o_busy) {
			/* nobody is reading: keep the newest output */
			s->o_wdeq++;
		} else if (s->stalled) {
			s->n_drop++;
			return;
		} else {
			dbc_drain(d, 2000);
			if (s->o_wenq - s->o_wdeq >= DBC_OBUF_SIZE) {
				s->n_drop++;
				return;
			}
		}
	}
	d->mem[DBC_OFF_OBUF + (s->o_wenq & (DBC_OBUF_SIZE - 1))] = c;
	DBC_FENCE();
	s->o_wenq++;
	if (c == '\n') {
		if (!s->stalled)
			dbc_drain(d, 20000);
		else
			dbc_poll(d);
	} else if (!s->o_busy && (s->cfg || (++s->n_idle & 63) == 0)) {
		dbc_poll(d);
	}
}

static inline void dbc_putc(struct dbc *d, uint8_t c)
{
	uint32_t f;

	if (d->s->suspended)
		return;
	dbc_lock(d, &f);
	dbc_putc_locked(d, c);
	dbc_unlock(d, f);
}

/* -1 when nothing was received */
static inline int dbc_getc(struct dbc *d)
{
	struct dbc_state *s = d->s;
	uint32_t f;
	int c = -1;

	if (s->suspended)
		return -1;
	dbc_lock(d, &f);
	if (s->i_wdeq == s->i_wenq)
		dbc_poll(d);
	if (s->i_wdeq != s->i_wenq)
		c = d->mem[DBC_OFF_IBUF + s->i_wdeq++];
	dbc_unlock(d, f);
	return c;
}

/* Bring the DbC up from a zeroed block (loader) and report whether a host is attached. */
static inline int dbc_start(struct dbc *d, uint32_t wait_ms)
{
	uint32_t i;

	if (!dbc_setup(d))
		return -1;
	for (i = 0; i < wait_ms * 10; i++) {
		dbc_poll(d);
		if (d->s->cfg)
			return 1;
		DBC_UDELAY(100);
	}
	return 0;
}

/* The kernel is about to reset the host controller, and has done so. */
static inline void dbc_hc_reset_begin(struct dbc *d)
{
	d->s->suspended = 1;
}

static inline void dbc_hc_reset_end(struct dbc *d)
{
	d->s->n_reinit++;
	dbc_setup(d);
	d->s->suspended = 0;
}

#endif

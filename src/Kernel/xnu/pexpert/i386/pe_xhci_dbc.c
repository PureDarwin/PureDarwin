// pe_xhci_dbc.c - polled serial console over the xHCI Debug Capability, for x86_64
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause
/*
 * Continues the DbC the EFI loader started: the loader passes the physical
 * address of its state in the pd_dbc boot-arg, and this maps it through the
 * physical map and keeps transmitting on the bulk rings without
 * reinitialising the controller. It is used next to the 16550 in pe_serial.c.
 * Based on the kernel side of Hiroki Sato's FreeBSD DbC work (xhci_dbc.c,
 * branch stable-14-hrs-usbdbc).
 *
 * Copyright (c) 2019-2024 Hiroki Sato <hrs@FreeBSD.org>
 * Copyright (c) 2026 The PureDarwin Project
 *
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

#include <machine/machine_routines.h>
#include <pexpert/protos.h>
#include <pexpert/pexpert.h>

extern uint64_t physmap_base;
extern uint64_t physmap_max;

static inline uint64_t
dbc_irq_save(void)
{
	uint64_t f;

	__asm__ volatile ("pushf; pop %0; cli" : "=r" (f) : : "memory");
	return f;
}

static inline void
dbc_irq_restore(uint64_t f)
{
	__asm__ volatile ("push %0; popf" : : "r" (f) : "memory", "cc");
}

static inline void
dbc_udelay(uint32_t us)
{
	/* an ISA port write takes about a microsecond, and there is no clock this early */
	while (us--) {
		__asm__ volatile ("outb %%al, $0x80" : : "a" (0));
	}
}

#define DBC_UDELAY(us)     dbc_udelay(us)
#define DBC_IRQ_SAVE()     dbc_irq_save()
#define DBC_IRQ_RESTORE(f) dbc_irq_restore(f)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunsafe-buffer-usage"
#pragma clang diagnostic ignored "-Wcast-align"
#include "xhci_dbc_core.h"

static uint64_t dbc_state_pa;           /* from the pd_dbc boot-arg, 0 when none */
static int dbc_checked;                 /* the boot-arg was looked at */
static int dbc_ok;                      /* the state is valid and bound */

/*
 * Early boot calls serial_init() before the platform expert exists, when the
 * command line is only reachable through the boot_args the bootstrap passed.
 * vstart() hands it over here.
 */
void pd_dbc_set_cmdline(const char *cmdline);

void
pd_dbc_set_cmdline(const char *cmdline)
{
	const char key[] = "pd_dbc=";
	const char *p;
	uint64_t v = 0;
	int i;

	if (dbc_checked || cmdline == NULL) {
		return;
	}
	for (p = cmdline; *p; p++) {
		if (p != cmdline && p[-1] != ' ') {
			continue;
		}
		for (i = 0; key[i] && p[i] == key[i]; i++) {
		}
		if (key[i]) {
			continue;
		}
		p += i;
		if (p[0] == '0' && (p[1] == 'x' || p[1] == 'X')) {
			p += 2;
			for (; (*p >= '0' && *p <= '9') || (*p >= 'a' && *p <= 'f') || (*p >= 'A' && *p <= 'F'); p++) {
				v = (v << 4) | (uint64_t)(*p <= '9' ? *p - '0' : (*p | 0x20) - 'a' + 10);
			}
		} else {
			for (; *p >= '0' && *p <= '9'; p++) {
				v = v * 10 + (uint64_t)(*p - '0');
			}
		}
		dbc_state_pa = v;
		break;
	}
	dbc_checked = 1;
}

/* The physical map moves while the kernel boots, so addresses are made fresh on every use. */
static int
dbc_bind(struct dbc *d)
{
	struct dbc_state *s;
	uint64_t lim;

	if (!dbc_ok) {
		return 0;
	}
	lim = physmap_max - physmap_base;
	s = (struct dbc_state *)(uintptr_t)(physmap_base + dbc_state_pa);
	if (s->mmio + s->dbc_off + 0x40 > lim) {
		return 0;
	}
	d->s = s;
	d->mem = (uint8_t *)(uintptr_t)(physmap_base + dbc_state_pa);
	d->regs = (volatile uint8_t *)(uintptr_t)(physmap_base + s->mmio + s->dbc_off);
	return 1;
}

/* Called from serial_init(): 1 when a DbC from the loader is usable. */
int pd_dbc_init(void);

int
pd_dbc_init(void)
{
	struct dbc d;
	struct dbc_state *s;
	uint64_t lim;

	if (dbc_ok) {
		/* resume from sleep: the controller may have lost the DbC */
		if (dbc_bind(&d) && dbc_rd(&d, DC_CTRL) != 0xffffffffu) {
			dbc_poll(&d);
		}
		return 1;
	}
	if (!dbc_checked && PE_state.bootArgs != NULL) {
		uint64_t v = 0;

		if (PE_parse_boot_argn("pd_dbc", &v, sizeof(v))) {
			dbc_state_pa = v;
		}
		dbc_checked = 1;
	}
	if (dbc_state_pa == 0 || (dbc_state_pa & 0xfff)) {
		return 0;
	}
	lim = physmap_max - physmap_base;
	if (dbc_state_pa + DBC_BLOCK_SIZE > lim) {
		return 0;
	}
	s = (struct dbc_state *)(uintptr_t)(physmap_base + dbc_state_pa);
	if (s->magic != DBC_MAGIC || s->version != DBC_VERSION || s->base != dbc_state_pa ||
	    s->mmio + s->dbc_off + 0x40 > lim) {
		return 0;
	}
	dbc_ok = 1;
	if (!dbc_bind(&d) || dbc_rd(&d, DC_CTRL) == 0xffffffffu) {
		dbc_ok = 0;
		return 0;
	}
	s->lock = 0;
	s->suspended = 0;
	dbc_poll(&d);           /* rebuilds the DbC if it was lost between the loader and here */
	return 1;
}

void pd_dbc_putc(char c);
int pd_dbc_getc(void);

void
pd_dbc_putc(char c)
{
	struct dbc d;

	if (dbc_bind(&d)) {
		dbc_putc(&d, (uint8_t)c);
	}
}

int
pd_dbc_getc(void)
{
	struct dbc d;

	return dbc_bind(&d) ? dbc_getc(&d) : -1;
}

/*
 * A host controller reset clears the DbC registers. The USB host driver calls
 * these around HCRST of the controller that carries the DbC (same PCI address
 * as the loader found it at): begin returns 1 for that controller, and end
 * then rebuilds the DbC from the saved state.
 */
int pd_dbc_hc_reset_begin(uint32_t rid);
void pd_dbc_hc_reset_end(void);

int
pd_dbc_hc_reset_begin(uint32_t rid)
{
	struct dbc d;

	if (!dbc_bind(&d) || d.s->rid != rid) {
		return 0;
	}
	dbc_hc_reset_begin(&d);
	return 1;
}

void
pd_dbc_hc_reset_end(void)
{
	struct dbc d;

	if (dbc_bind(&d) && d.s->suspended) {
		dbc_hc_reset_end(&d);
	}
}
#pragma clang diagnostic pop

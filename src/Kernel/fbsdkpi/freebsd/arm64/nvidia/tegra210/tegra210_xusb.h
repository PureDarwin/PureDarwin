/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2016 Michal Meloun <mmel@FreeBSD.org>
 * Copyright 2020 Michal Meloun <mmel@FreeBSD.org>
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

/*
 * PureDarwin: the interface of tegra210_xusb.c, excerpts of FreeBSD's
 * tegra_xhci.c, tegra210_xusbpadctl.c and the xusb parts of the tegra210
 * clock code (freebsd-src main), with busdma, clk, hwreset, phy and
 * regulator frameworks replaced by direct register access.
 */

#ifndef _TEGRA210_XUSB_H_
#define	_TEGRA210_XUSB_H_

#include <sys/types.h>

struct tegra210_xusb_bus {
	volatile uint32_t	*car;		/* CAR at 0x60006000 */
	volatile uint32_t	*padctl;	/* xusb padctl */
	volatile uint32_t	*fpci;		/* xusb host fpci config space */
	volatile uint32_t	*ipfs;		/* xusb host ipfs */
	volatile uint32_t	*fuse;		/* efuse block, NULL when unclocked */
	void			(*log)(const char *fmt, ...);
};

/* Falcon firmware image header, from tegra_xhci.c. */
struct tegra_xusb_fw_hdr {
	uint32_t	boot_loadaddr_in_imem;
	uint32_t	boot_codedfi_offset;
	uint32_t	boot_codetag;
	uint32_t	boot_codesize;

	uint32_t	phys_memaddr;
	uint16_t	reqphys_memsize;
	uint16_t	alloc_phys_memsize;

	uint32_t	rodata_img_offset;
	uint32_t	rodata_section_start;
	uint32_t	rodata_section_end;
	uint32_t	main_fnaddr;

	uint32_t	fwimg_cksum;
	uint32_t	fwimg_created_time;

	uint32_t	imem_resident_start;
	uint32_t	imem_resident_end;
	uint32_t	idirect_start;
	uint32_t	idirect_end;
	uint32_t	l2_imem_start;
	uint32_t	l2_imem_end;
	uint32_t	version_id;
	uint8_t		init_ddirect;
	uint8_t		reserved[3];
	uint32_t	phys_addr_log_buffer;
	uint32_t	total_log_entries;
	uint32_t	dequeue_ptr;
	uint32_t	dummy[2];
	uint32_t	fwimg_len;
	uint8_t		magic[8];
	uint32_t	ss_low_power_entry_timeout;
	uint8_t		num_hsic_port;
	uint8_t		ss_portmap;
	uint8_t		build;
	uint8_t		padding[137]; /* Pad to 256 bytes */
};

void	tegra210_xusb_attach(const struct tegra210_xusb_bus *bus);

/* Clocks and PLLs: tegra210_car.c init table, tegra210_clk_pll.c. */
void	tegra210_xusb_dump_car(const char *when);
int	tegra210_xusb_setup_clocks(void);

/* Pad controller: tegra210_xusbpadctl.c. */
void	tegra210_xusb_dump_padctl(const char *when);
void	tegra210_xusb_padctl_calibration(void);
int	tegra210_xusb_padctl_usb2_port(int idx);
void	tegra210_xusb_padctl_common(void);

/* Host: tegra_xhci.c init_hw, load_fw and the mailbox. */
void	tegra210_xusb_init_ipfs(uint64_t xhci_base);
int	tegra210_xusb_fw_loaded(void);
int	tegra210_xusb_load_fw(const void *fw_vaddr, uint64_t fw_paddr);
int	tegra210_xusb_mbox_send_cmd(uint32_t cmd, uint32_t data);
int	tegra210_xusb_mbox_poll(void);
uint32_t tegra210_xusb_falcon_cpuctl(void);

#endif /* _TEGRA210_XUSB_H_ */

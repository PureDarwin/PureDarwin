/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright 2020 Michal Meloun <mmel@FreeBSD.org>
 * Copyright (c) 2015 Michal Meloun
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
 * PureDarwin: excerpts of FreeBSD freebsd-src e6fb3b51
 *   sys/arm64/nvidia/tegra210/tegra210_car.c, tegra210_car.h  (osc, clk_m)
 *   sys/arm64/nvidia/tegra210/tegra210_clk_per.c  (pgate, hwreset)
 *   sys/arm64/nvidia/tegra210/tegra210_clk_pll.c  (pllP_out5 DIV7_1, GATE_PLL)
 *   sys/dev/clk/clk_div.c, clk_gate.c             (divider and gate math)
 *   sys/arm64/nvidia/tegra210/tegra210_pmc.c      (powergate, clamping)
 *   sys/arm/nvidia/tegra_efuse.c                  (tegra210 speedo)
 * The clknode/hwreset framework, softc and locking are replaced by direct
 * register access through struct tegra210_gpu_bus. Callers serialize.
 */

#include <sys/param.h>
#include <sys/systm.h>

#include <arm/nvidia/tegra_pmc.h>
#include "tegra210_gpu.h"

/* tegra210_car.h */
#define	RST_DEVICES_L			0x004
#define	RST_DEVICES_H			0x008
#define	RST_DEVICES_U			0x00C
#define	CLK_OUT_ENB_L			0x010
#define	CLK_OUT_ENB_H			0x014
#define	CLK_OUT_ENB_U			0x018
#define	OSC_CTRL			0x050
#define	 OSC_CTRL_OSC_FREQ_GET(x)	(((x) >> 28) & 0x0F)
#define	 OSC_CTRL_PLL_REF_DIV_GET(x)	(((x) >> 26) & 0x03)
#define	RST_DEVICES_V			0x358
#define	RST_DEVICES_W			0x35C
#define	CLK_OUT_ENB_V			0x360
#define	CLK_OUT_ENB_W			0x364
#define	CLK_OUT_ENB_X			0x280
#define	RST_DEVICES_X			0x28C
#define	CLK_OUT_ENB_Y			0x298
#define	RST_DEVICES_Y			0x2a4
#define	SPARE_REG0			0x55c
#define	 SPARE_REG0_MDIV_GET(x)		(((x) >> 2) & 0x03)
#define	PLLP_OUTC			0x67c

/* tegra210_pmc.c */
#define	PMC_CLAMP_STATUS		0x02C
#define	  PMC_CLAMP_STATUS_PARTID(x)		(1 << ((x) & 0x1F))
#define	PMC_PWRGATE_TOGGLE		0x030
#define	 PMC_PWRGATE_TOGGLE_START		(1 << 8)
#define	 PMC_PWRGATE_TOGGLE_PARTID(x)		(((x) & 0x1F) << 0)
#define	PMC_REMOVE_CLAMPING_CMD		0x034
#define	  PMC_REMOVE_CLAMPING_CMD_PARTID(x)	(1 << ((x) & 0x1F))
#define	PMC_PWRGATE_STATUS		0x038
#define	PMC_PWRGATE_STATUS_PARTID(x)		(1 << ((x) & 0x1F))
#define	PMC_GPU_RG_CNTRL		0x2d4

/* tegra_efuse.c */
#define	TEGRA210_CPU_PROCESS_CORNERS	2
#define	TEGRA210_GPU_PROCESS_CORNERS	2
#define	TEGRA210_FUSE_SKU_INFO		0x010
#define	TEGRA210_FUSE_CPU_SPEEDO_2	0x030
#define	TEGRA210_FUSE_GPU_IDDQ		0x128
#define	TEGRA210_FUSE_SPARE		0x270
#define	FUSES_START			0x100

static struct tegra210_gpu_bus bus;

void
tegra210_gpu_attach(const struct tegra210_gpu_bus *b)
{
	bus = *b;
}

static uint32_t
CAR_RD4(uint32_t r)
{
	return (bus.car[r / 4]);
}

static void
CAR_MD4(uint32_t r, uint32_t clr_mask, uint32_t set_mask)
{
	uint32_t reg;

	reg = bus.car[r / 4];
	reg &= ~clr_mask;
	reg |= set_mask;
	bus.car[r / 4] = reg;
	__asm__ volatile ("dsb sy" ::: "memory");
}

/* tegra210_car.c: osc, osc_div_clk and clk_m */
static uint32_t osc_freqs[16] = {
	 [0] =  13000000,
	 [1] =  16800000,
	 [4] =  19200000,
	 [5] =  38400000,
	 [8] =  12000000,
	 [9] =  48000000,
};

uint64_t
tegra210_osc_freq(void)
{
	return (osc_freqs[OSC_CTRL_OSC_FREQ_GET(CAR_RD4(OSC_CTRL))]);
}

uint64_t
tegra210_osc_div_freq(void)
{
	return (tegra210_osc_freq() >>
	    OSC_CTRL_PLL_REF_DIV_GET(CAR_RD4(OSC_CTRL)));
}

uint64_t
tegra210_clk_m_freq(void)
{
	return (tegra210_osc_freq() /
	    (SPARE_REG0_MDIV_GET(CAR_RD4(SPARE_REG0)) + 1));
}

/* tegra210_clk_per.c: peripheral gates and resets */
static uint32_t clk_enable_reg[] = {
	CLK_OUT_ENB_L,
	CLK_OUT_ENB_H,
	CLK_OUT_ENB_U,
	CLK_OUT_ENB_V,
	CLK_OUT_ENB_W,
	CLK_OUT_ENB_X,
	CLK_OUT_ENB_Y,
};

static uint32_t clk_reset_reg[] = {
	RST_DEVICES_L,
	RST_DEVICES_H,
	RST_DEVICES_U,
	RST_DEVICES_V,
	RST_DEVICES_W,
	RST_DEVICES_X,
	RST_DEVICES_Y,
};

int
tegra210_pgate_set_gate(uint32_t idx, bool enable)
{
	uint32_t mask;

	if (idx / 32 >= nitems(clk_enable_reg))
		return (EINVAL);
	mask = 1 << (idx % 32);
	CAR_MD4(clk_enable_reg[idx / 32], mask, enable ? mask : 0);
	DELAY(2);
	return (0);
}

int
tegra210_pgate_get_gate(uint32_t idx, bool *enabled)
{
	if (idx / 32 >= nitems(clk_enable_reg))
		return (EINVAL);
	*enabled = CAR_RD4(clk_enable_reg[idx / 32]) & (1 << (idx % 32)) ?
	    true : false;
	return (0);
}

int
tegra210_hwreset_by_idx(uint32_t idx, bool reset)
{
	uint32_t mask;

	if (idx / 32 >= nitems(clk_reset_reg))
		return (EINVAL);
	mask = 1 << (idx % 32);
	CAR_MD4(clk_reset_reg[idx / 32], mask, reset ? mask : 0);
	return (0);
}

int
tegra210_hwreset_is_asserted(uint32_t idx)
{
	if (idx / 32 >= nitems(clk_reset_reg))
		return (-1);
	return (CAR_RD4(clk_reset_reg[idx / 32]) & (1 << (idx % 32)) ? 1 : 0);
}

/*
 * tegra210_clk_pll.c: DIV7_1(0, "pllP_out5_div", "pllP_out0", PLLP_OUTC, 24)
 * and GATE_PLL(0, "pllP_out5", "pllP_out5_div", PLLP_OUTC, 16), with the
 * fractional divider math of clk_div.c (i_shift 25, i_width 7, f_shift 24,
 * f_width 1, not zero based).
 */
#define	OUT5_I_SHIFT	25
#define	OUT5_I_MASK	0x7f
#define	OUT5_F_SHIFT	24
#define	OUT5_F_MASK	0x1
#define	OUT5_F_WIDTH	1
#define	OUT5_GATE_SHIFT	16
#define	OUT5_GATE_MASK	3

uint64_t
tegra210_pllp_out5_get_freq(uint64_t fin)
{
	uint32_t reg, i_div, f_div, divider;

	reg = CAR_RD4(PLLP_OUTC);
	i_div = ((reg >> OUT5_I_SHIFT) & OUT5_I_MASK) + 1;
	f_div = (reg >> OUT5_F_SHIFT) & OUT5_F_MASK;
	divider = i_div << OUT5_F_WIDTH | f_div;
	return ((fin << OUT5_F_WIDTH) / divider);
}

int
tegra210_pllp_out5_set_freq(uint64_t fin, uint64_t fout)
{
	uint64_t divider, _fin;
	uint32_t i_div, f_div, hw_i_div;

	_fin = fin << OUT5_F_WIDTH;
	divider = (_fin + fout / 2) / fout;
	i_div = divider >> OUT5_F_WIDTH;
	f_div = divider & OUT5_F_MASK;
	if (i_div == 0 || i_div - 1 > OUT5_I_MASK)
		return (ERANGE);
	hw_i_div = i_div - 1;
	if (fout != _fin / divider)
		return (ERANGE);
	CAR_MD4(PLLP_OUTC,
	    (OUT5_I_MASK << OUT5_I_SHIFT) | (OUT5_F_MASK << OUT5_F_SHIFT),
	    (hw_i_div << OUT5_I_SHIFT) | (f_div << OUT5_F_SHIFT));
	return (0);
}

int
tegra210_pllp_out5_set_gate(bool enable)
{
	CAR_MD4(PLLP_OUTC, OUT5_GATE_MASK << OUT5_GATE_SHIFT,
	    (enable ? 3 : 0) << OUT5_GATE_SHIFT);
	return (0);
}

/* tegra210_pmc.c: powergates and clamps, the PMC reached through bus */
static void
WR4(uint32_t r, uint32_t v)
{
	bus.pmc_write(r, v);
}

static uint32_t
RD4(uint32_t r)
{
	return (bus.pmc_read(r));
}

static int
tegra210_pmc_set_powergate(enum tegra_powergate_id id, int ena)
{
	uint32_t reg;
	int i;

	reg = RD4(PMC_PWRGATE_STATUS) & PMC_PWRGATE_STATUS_PARTID(id);
	if (((reg != 0) && ena) || ((reg == 0) && !ena))
		return (0);

	for (i = 100; i > 0; i--) {
		reg = RD4(PMC_PWRGATE_TOGGLE);
		if ((reg & PMC_PWRGATE_TOGGLE_START) == 0)
			break;
		DELAY(1);
	}
	if (i <= 0)
		printf("tegra210_pmc: Timeout when waiting for TOGGLE_START\n");

	WR4(PMC_PWRGATE_TOGGLE,
	    PMC_PWRGATE_TOGGLE_START | PMC_PWRGATE_TOGGLE_PARTID(id));

	for (i = 100; i > 0; i--) {
		reg = RD4(PMC_PWRGATE_TOGGLE);
		if ((reg & PMC_PWRGATE_TOGGLE_START) == 0)
			break;
		DELAY(1);
	}
	if (i <= 0)
		printf("tegra210_pmc: Timeout when waiting for TOGGLE_START\n");
	return (0);
}

int
tegra_powergate_remove_clamping(enum tegra_powergate_id id)
{
	uint32_t reg;
	enum tegra_powergate_id swid;
	int i;

	if (id == TEGRA_POWERGATE_3D) {
		WR4(PMC_GPU_RG_CNTRL, 0);
		return (0);
	}

	reg = RD4(PMC_PWRGATE_STATUS);
	if ((reg & PMC_PWRGATE_STATUS_PARTID(id)) == 0) {
		/* PureDarwin: FreeBSD panics here. */
		printf("tegra210_pmc: clamp removal on unpowered partition\n");
		return (ENXIO);
	}

	if (id == TEGRA_POWERGATE_PCX)
		swid = TEGRA_POWERGATE_VDE;
	else if (id == TEGRA_POWERGATE_VDE)
		swid = TEGRA_POWERGATE_PCX;
	else
		swid = id;
	WR4(PMC_REMOVE_CLAMPING_CMD, PMC_REMOVE_CLAMPING_CMD_PARTID(swid));

	for (i = 100; i > 0; i--) {
		reg = RD4(PMC_REMOVE_CLAMPING_CMD);
		if ((reg & PMC_REMOVE_CLAMPING_CMD_PARTID(swid)) == 0)
			break;
		DELAY(1);
	}
	if (i <= 0)
		printf("tegra210_pmc: Timeout when remove clamping\n");

	reg = RD4(PMC_CLAMP_STATUS);
	if ((reg & PMC_CLAMP_STATUS_PARTID(id)) != 0) {
		/* PureDarwin: FreeBSD panics here. */
		printf("tegra210_pmc: Cannot remove clamping\n");
		return (EIO);
	}
	return (0);
}

int
tegra_powergate_is_powered(enum tegra_powergate_id id)
{
	uint32_t reg;

	reg = RD4(PMC_PWRGATE_STATUS);
	return ((reg & PMC_PWRGATE_STATUS_PARTID(id)) ? 1 : 0);
}

int
tegra_powergate_power_on(enum tegra_powergate_id id)
{
	int rv, i;

	rv = tegra210_pmc_set_powergate(id, 1);
	if (rv != 0) {
		printf("tegra210_pmc: Cannot set powergate: %d\n", id);
		return (rv);
	}

	for (i = 100; i > 0; i--) {
		if (tegra_powergate_is_powered(id))
			break;
		DELAY(1);
	}
	if (i <= 0) {
		printf("tegra210_pmc: Timeout when waiting on power up\n");
		return (ETIMEDOUT);
	}
	return (rv);
}

/* tegra_efuse.c: the Tegra 210 sku and gpu speedo */
static uint32_t tegra210_gpu_process_speedos[][TEGRA210_GPU_PROCESS_CORNERS] =
{
	{UINT_MAX, UINT_MAX},
	{UINT_MAX, UINT_MAX},
};

static uint32_t
FUSE_RD4(uint32_t r)
{
	return (bus.fuse[(FUSES_START + r) / 4]);
}

static uint32_t
tegra210_get_speedo_revision(void)
{
	uint32_t reg;
	uint32_t val;

	val = 0;

	/* Revision i encoded in spare fields */
	reg = FUSE_RD4(TEGRA210_FUSE_SPARE + 2 * 4);
	val |=  (reg & 1) << 0;
	reg = FUSE_RD4(TEGRA210_FUSE_SPARE + 3 * 4);
	val |=  (reg & 1) << 1;
	reg = FUSE_RD4(TEGRA210_FUSE_SPARE + 4 * 4);
	val |=  (reg & 1) << 2;

	return (val);
}

int
tegra210_efuse_sku(struct tegra210_sku *sku)
{
	uint32_t cpu_speedo2;
	int i, threshold, speedo_rev;

	if (bus.fuse == NULL)
		return (ENXIO);
	sku->sku_id = FUSE_RD4(TEGRA210_FUSE_SKU_INFO);
	cpu_speedo2 = FUSE_RD4(TEGRA210_FUSE_CPU_SPEEDO_2);
	sku->gpu_iddq_value = FUSE_RD4(TEGRA210_FUSE_GPU_IDDQ);

	speedo_rev = tegra210_get_speedo_revision();
	sku->speedo_rev = speedo_rev;
	if (speedo_rev >= 3)
		sku->gpu_speedo_value = cpu_speedo2;
	else if (speedo_rev == 2)
		sku->gpu_speedo_value =
		    (-1662 + (1082 * cpu_speedo2 / 100)) / 10;
	else
		sku->gpu_speedo_value = cpu_speedo2 - 75;

	/* tegra210_rev_sku_to_speedo_ids(), gpu part */
	sku->gpu_speedo_id = 0;
	threshold = 0;
	switch (sku->sku_id) {
	case 0x00: /* Eng sku */
	case 0x01: /* Eng sku */
	case 0x07:
	case 0x17:
	case 0x27:
	case 0x13:
		if (speedo_rev >= 2)
			sku->gpu_speedo_id = 1;
		break;
	default:
		printf("tegra210_efuse: Unknown SKU ID %d\n", sku->sku_id);
		break;
	}

	for (i = 0; i < TEGRA210_GPU_PROCESS_CORNERS; i++) {
		if (sku->gpu_speedo_value <
			tegra210_gpu_process_speedos[threshold][i])
			break;
	}
	sku->gpu_process_id = i;
	return (0);
}

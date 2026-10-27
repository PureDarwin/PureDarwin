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
 * PureDarwin: excerpts of FreeBSD freebsd-src main
 *   sys/arm/nvidia/tegra_xhci.c                  (init_hw, load_fw, mailbox)
 *   sys/arm64/nvidia/tegra210/tegra210_xusbpadctl.c  (usb2 lanes, calibration)
 *   sys/arm64/nvidia/tegra210/tegra210_car.c     (xusb clock init table)
 *   sys/arm64/nvidia/tegra210/tegra210_clk_pll.c (pllU gates)
 *   sys/arm64/nvidia/tegra210/tegra210_clk_per.c (xusb clock sources)
 * The device, busdma and clk/hwreset/phy/regulator frameworks are replaced by
 * direct register access through struct tegra210_xusb_bus. Callers
 * serialize. Values PureDarwin adds are marked with their source.
 */

#include <sys/param.h>
#include <sys/systm.h>

#include "tegra210_xusb.h"

static struct tegra210_xusb_bus bus;

#define	LOG(...)	do { if (bus.log != NULL) bus.log(__VA_ARGS__); } while (0)

void
tegra210_xusb_attach(const struct tegra210_xusb_bus *b)
{
	bus = *b;
}

static uint32_t
rd(volatile uint32_t *base, uint32_t r)
{
	return (base[r / 4]);
}

static void
wr(volatile uint32_t *base, uint32_t r, uint32_t v)
{
	base[r / 4] = v;
	__asm__ volatile ("dsb sy" ::: "memory");
}

/* -------------------------------------------------------------------------
 * tegra210_car.h, tegra210_clk_pll.c, tegra210_clk_per.c
 */
#define	PLLU_BASE			0x0c0
#define	PLLU_OUTA			0x0c4
#define	PLLU_MISC			0x0cc
#define	CLK_OUT_ENB_U			0x018
#define	RST_DEVICES_U			0x00C
#define	CLK_OUT_ENB_W			0x364
#define	RST_DEVICES_W			0x35C
#define	CLK_OUT_ENB_Y			0x298
#define	RST_DEVICES_Y			0x2a4
#define	UTMIP_PLL_CFG0			0x480
#define	UTMIP_PLL_CFG1			0x484
#define	UTMIP_PLL_CFG2			0x488
#define	UTMIPLL_HW_PWRDN_CFG0		0x52c
#define	 UTMIPLL_HW_PWRDN_CFG0_UTMIPLL_LOCK		(1U << 31)
#define	PLLU_HW_PWRDN_CFG0		0x530
#define	CLK_SOURCE_XUSB_CORE_HOST	0x600
#define	CLK_SOURCE_XUSB_FALCON		0x604
#define	CLK_SOURCE_XUSB_FS		0x608
#define	CLK_SOURCE_XUSB_CORE_DEV	0x60c
#define	CLK_SOURCE_XUSB_SS		0x610
#define	PLLP_MISC1			0x680
#define	CLK_SOURCE_USB2_HSIC_TRK	0x6cc

#define	PERLCK_MUX_SHIFT		29
#define	PERLCK_MUX_MASK			0x07

/* GATE(0, "pllU_480", "pllU", PLLU_BASE, 22) and the 60 and 48 MHz gates. */
#define	PLLU_GATE_480			(1 << 22)
#define	PLLU_GATE_60			(1 << 23)
#define	PLLU_GATE_48			(1 << 25)
/* GATE_PLL(0, "pllU_out1"/"pllU_out2", ..., PLLU_OUTA, 0 / 16), mask 3. */
#define	PLLU_OUTA_GATES			((3 << 0) | (3 << 16))
/* GATE(0, "pllP_hsio"/"pllP_xusb", ..., PLLP_MISC1, 29 / 28). */
#define	PLLP_MISC1_HSIO_XUSB		((1 << 29) | (1 << 28))

/* Mux indexes in mux_clkm_p_N_N_N_refre, mux_clkm_N_u48_N_p_N_u480, mux_clkm_refe_clks_u480. */
#define	MUX_PLLP_XUSB			1
#define	MUX_PLLU_48			2
#define	MUX_PLLU_480			3
/* MUX(TEGRA210_CLK_XUSB_HS_SRC, "xusb_hs", ..., CLK_SOURCE_XUSB_SS, 25, 2), "pc_xusb_ss" is 2. */
#define	XUSB_HS_SHIFT			25
#define	XUSB_HS_MASK			0x3
#define	XUSB_HS_SS			2
/* MUX(0, "xusb_ssp", ..., CLK_SOURCE_XUSB_SS, 24, 1), "xusb_ss" is 0. */
#define	XUSB_SSP_SHIFT			24

static void
car_md(uint32_t r, uint32_t clr, uint32_t set)
{
	wr(bus.car, r, (rd(bus.car, r) & ~clr) | set);
}

/* An 8.1 fractional divider: the field is 2 * fin / fout - 2 (clk_div.c, not zero based). */
static uint32_t
div_8_1(uint64_t fin, uint64_t fout)
{
	return ((uint32_t)((fin * 2) / fout - 2) & 0xff);
}

static void
set_per_clk(uint32_t r, uint32_t mux, uint64_t fin, uint64_t fout)
{
	car_md(r, (PERLCK_MUX_MASK << PERLCK_MUX_SHIFT) | 0xff,
	    (mux << PERLCK_MUX_SHIFT) | div_8_1(fin, fout));
}

void
tegra210_xusb_dump_car(const char *when)
{
	LOG("car %s: pllu base 0x%08x outa 0x%08x misc 0x%08x hw 0x%08x, utmip cfg0-2 0x%08x 0x%08x 0x%08x hw 0x%08x\n",
	    when, rd(bus.car, PLLU_BASE), rd(bus.car, PLLU_OUTA), rd(bus.car, PLLU_MISC),
	    rd(bus.car, PLLU_HW_PWRDN_CFG0), rd(bus.car, UTMIP_PLL_CFG0), rd(bus.car, UTMIP_PLL_CFG1),
	    rd(bus.car, UTMIP_PLL_CFG2), rd(bus.car, UTMIPLL_HW_PWRDN_CFG0));
	LOG("car %s: enb u 0x%08x w 0x%08x y 0x%08x, rst u 0x%08x w 0x%08x y 0x%08x, pllp misc1 0x%08x\n",
	    when, rd(bus.car, CLK_OUT_ENB_U), rd(bus.car, CLK_OUT_ENB_W), rd(bus.car, CLK_OUT_ENB_Y),
	    rd(bus.car, RST_DEVICES_U), rd(bus.car, RST_DEVICES_W), rd(bus.car, RST_DEVICES_Y),
	    rd(bus.car, PLLP_MISC1));
	LOG("car %s: src host 0x%08x falcon 0x%08x fs 0x%08x dev 0x%08x ss 0x%08x trk 0x%08x\n",
	    when, rd(bus.car, CLK_SOURCE_XUSB_CORE_HOST), rd(bus.car, CLK_SOURCE_XUSB_FALCON),
	    rd(bus.car, CLK_SOURCE_XUSB_FS), rd(bus.car, CLK_SOURCE_XUSB_CORE_DEV),
	    rd(bus.car, CLK_SOURCE_XUSB_SS), rd(bus.car, CLK_SOURCE_USB2_HSIC_TRK));
}

/*
 * tegra210_car.c clk_init_table, xusb rows:
 *   {"xusb_ss", "pllU_480", 120000000}, {"pc_xusb_fs", "pllU_48", 48000000},
 *   {"xusb_hs", "pc_xusb_ss", 120000000}, {"xusb_ssp", "xusb_ss", 120000000},
 *   {"pc_xusb_falcon", "pllP_xusb", 204000000},
 *   {"pc_xusb_core_host", "pllP_xusb", 102000000},
 *   {"pc_xusb_core_dev", "pllP_xusb", 102000000},
 *   {"pllU_out1", NULL, 48000000, 1}, {"pllU_out2", NULL, 60000000, 1}
 * PLLU and UTMIPLL themselves are left as the boot firmware set them up, the
 * usb device controller already runs from them.
 */
int
tegra210_xusb_setup_clocks(void)
{
	const uint64_t pllp = 408000000, pllu = 480000000;

	if ((rd(bus.car, PLLU_BASE) & (1U << 27)) == 0) {
		/* PureDarwin: PLLU_LOCK is bit 27 (tegra x1 trm 5.2.34) */
		LOG("pllU is not locked, the boot firmware did not start it\n");
		return (ENXIO);
	}
	if ((rd(bus.car, UTMIPLL_HW_PWRDN_CFG0) & UTMIPLL_HW_PWRDN_CFG0_UTMIPLL_LOCK) == 0)
		LOG("utmipll reports no lock\n");

	car_md(PLLU_BASE, 0, PLLU_GATE_480 | PLLU_GATE_60 | PLLU_GATE_48);
	car_md(PLLU_OUTA, 0, PLLU_OUTA_GATES);
	car_md(PLLP_MISC1, 0, PLLP_MISC1_HSIO_XUSB);
	DELAY(2);

	set_per_clk(CLK_SOURCE_XUSB_SS, MUX_PLLU_480, pllu, 120000000);
	car_md(CLK_SOURCE_XUSB_SS, (XUSB_HS_MASK << XUSB_HS_SHIFT) | (1 << XUSB_SSP_SHIFT),
	    XUSB_HS_SS << XUSB_HS_SHIFT);
	set_per_clk(CLK_SOURCE_XUSB_FS, MUX_PLLU_48, 48000000, 48000000);
	set_per_clk(CLK_SOURCE_XUSB_FALCON, MUX_PLLP_XUSB, pllp, 204000000);
	set_per_clk(CLK_SOURCE_XUSB_CORE_HOST, MUX_PLLP_XUSB, pllp, 102000000);
	set_per_clk(CLK_SOURCE_XUSB_CORE_DEV, MUX_PLLP_XUSB, pllp, 102000000);
	/* PureDarwin: tracking clock divisor 6 for a 30 to 40 MHz osc (tegra x1 trm 22.8.10) */
	car_md(CLK_SOURCE_USB2_HSIC_TRK, 0xff, 6);
	DELAY(2);
	return (0);
}

/* -------------------------------------------------------------------------
 * tegra210_xusbpadctl.c
 */
#define	FUSE_SKU_CALIB_0			0x0F0
#define	  FUSE_SKU_CALIB_0_HS_CURR_LEVEL_123(x, i)	(((x) >> (11 + ((i) - 1) * 6)) & 0x3F)
#define	  FUSE_SKU_CALIB_0_HS_TERM_RANGE_ADJ(x)		(((x) >>  7) & 0x0F)
#define	  FUSE_SKU_CALIB_0_HS_CURR_LEVEL_0(x)		(((x) >>  0) & 0x3F)

#define	FUSE_USB_CALIB_EXT_0			0x250
#define	  FUSE_USB_CALIB_EXT_0_RPD_CTRL(x)		(((x) >>  0) & 0x1F)

#define	FUSES_START				0x100

#define	XUSB_PADCTL_USB2_PAD_MUX		0x004
#define	XUSB_PADCTL_USB2_PORT_CAP		0x008
#define	 USB2_PORT_CAP_PORT_CAP(p, x)			(((x) & 3) << ((p) * 4))
#define	  USB2_PORT_CAP_PORT_CAP_HOST			0x1
#define	XUSB_PADCTL_SS_PORT_MAP			0x014

#define	XUSB_PADCTL_ELPG_PROGRAM1		0x024
#define	 ELPG_PROGRAM1_AUX_MUX_LP0_VCORE_DOWN		(1U << 31)
#define	 ELPG_PROGRAM1_AUX_MUX_LP0_CLAMP_EN_EARLY	(1 << 30)
#define	 ELPG_PROGRAM1_AUX_MUX_LP0_CLAMP_EN		(1 << 29)

#define	XUSB_PADCTL_USB2_BATTERY_CHRG_OTGPAD_CTL1(x) (0x084 + (x) * 0x40)
#define	 USB2_BATTERY_CHRG_OTGPAD_CTL1_VREG_LEV(x)	 (((x) & 0x3) <<  7)
#define	 USB2_BATTERY_CHRG_OTGPAD_CTL1_VREG_FIX18	 (1 <<  6)

#define	XUSB_PADCTL_USB2_OTG_PAD_CTL0(x) 	(0x088 + (x) * 0x40)
#define	 USB2_OTG_PAD_CTL0_PD_ZI			(1 << 29)
#define	 USB2_OTG_PAD_CTL0_PD2				(1 << 27)
#define	 USB2_OTG_PAD_CTL0_PD				(1 << 26)
#define	 USB2_OTG_PAD_CTL0_HS_SLEW(x)			(((x) & 0x3F) <<  6)
#define	 USB2_OTG_PAD_CTL0_HS_CURR_LEVEL(x)		(((x) & 0x3F) <<  0)

#define XUSB_PADCTL_USB2_OTG_PAD_CTL1(x) 	(0x08C + (x) * 0x40)
#define	 USB2_OTG_PAD_CTL1_RPD_CTRL(x)			(((x) & 0x1F) <<  26)
#define	 USB2_OTG_PAD_CTL1_TERM_RANGE_ADJ(x)		(((x) & 0x0F) <<  3)
#define	 USB2_OTG_PAD_CTL1_PD_DR			(1 <<  2)
#define	 USB2_OTG_PAD_CTL1_PD_DISC_OVRD			(1 <<  1)
#define	 USB2_OTG_PAD_CTL1_PD_CHRP_OVRD			(1 <<  0)

#define	XUSB_PADCTL_USB2_BIAS_PAD_CTL0		0x0284
#define	 USB2_BIAS_PAD_CTL0_PD				(1 << 11)
#define	 USB2_BIAS_PAD_CTL0_HS_DISCON_LEVEL(x)		(((x) & 0x7) <<  3)
#define	 USB2_BIAS_PAD_CTL0_HS_SQUELCH_LEVEL(x)		(((x) & 0x7) <<  0)

#define	XUSB_PADCTL_USB2_BIAS_PAD_CTL1		0x0288
#define	 USB2_BIAS_PAD_CTL1_PD_TRK			(1 << 26)
#define	 USB2_BIAS_PAD_CTL1_TRK_DONE_RESET_TIMER(x)	(((x) & 0x7F) << 19)
#define	 USB2_BIAS_PAD_CTL1_TRK_START_TIMER(x)		(((x) & 0x7F) << 12)

#define	USB2_LANE_MUX_SHIFT(p)			((p) * 2)
#define	USB2_LANE_MUX_MASK			0x3
#define	USB2_LANE_MUX_XUSB			1	/* usb_mux[] = {"snps", "xusb", ...} */

static struct {
	uint32_t	hs_curr_level[4];
	uint32_t	hs_curr_level_offs;	/* Not inited yet, always 0 */
	uint32_t	hs_term_range_adj;
	uint32_t	rpd_ctrl;
} cal;

void
tegra210_xusb_dump_padctl(const char *when)
{
	LOG("padctl %s: pad_mux 0x%08x port_cap 0x%08x ss_map 0x%08x elpg1 0x%08x bias0 0x%08x bias1 0x%08x\n",
	    when, rd(bus.padctl, XUSB_PADCTL_USB2_PAD_MUX), rd(bus.padctl, XUSB_PADCTL_USB2_PORT_CAP),
	    rd(bus.padctl, XUSB_PADCTL_SS_PORT_MAP), rd(bus.padctl, XUSB_PADCTL_ELPG_PROGRAM1),
	    rd(bus.padctl, XUSB_PADCTL_USB2_BIAS_PAD_CTL0), rd(bus.padctl, XUSB_PADCTL_USB2_BIAS_PAD_CTL1));
	for (int i = 0; i < 4; i++) {
		LOG("padctl %s: otg pad %d ctl0 0x%08x ctl1 0x%08x chrg1 0x%08x\n", when, i,
		    rd(bus.padctl, XUSB_PADCTL_USB2_OTG_PAD_CTL0(i)),
		    rd(bus.padctl, XUSB_PADCTL_USB2_OTG_PAD_CTL1(i)),
		    rd(bus.padctl, XUSB_PADCTL_USB2_BATTERY_CHRG_OTGPAD_CTL1(i)));
	}
}

/* load_calibration(), PureDarwin reads FUSE_USB_CALIB_EXT_0 into reg, FreeBSD drops it */
void
tegra210_xusb_padctl_calibration(void)
{
	uint32_t reg;
	int i;

	if (bus.fuse == NULL) {
		LOG("no fuse block, pad calibration left at 0\n");
		return;
	}
	reg = bus.fuse[(FUSES_START + FUSE_SKU_CALIB_0) / 4];
	cal.hs_curr_level[0] = FUSE_SKU_CALIB_0_HS_CURR_LEVEL_0(reg);
	for (i = 1; i < nitems(cal.hs_curr_level); i++) {
		cal.hs_curr_level[i] =
		    FUSE_SKU_CALIB_0_HS_CURR_LEVEL_123(reg, i);
	}
	cal.hs_term_range_adj = FUSE_SKU_CALIB_0_HS_TERM_RANGE_ADJ(reg);

	reg = bus.fuse[(FUSES_START + FUSE_USB_CALIB_EXT_0) / 4];
	cal.rpd_ctrl = FUSE_USB_CALIB_EXT_0_RPD_CTRL(reg);
	LOG("pad calibration: hs_curr_level %u %u %u %u, term_range_adj %u, rpd_ctrl %u\n",
	    cal.hs_curr_level[0], cal.hs_curr_level[1], cal.hs_curr_level[2], cal.hs_curr_level[3],
	    cal.hs_term_range_adj, cal.rpd_ctrl);
}

/* pad_common_enable() */
void
tegra210_xusb_padctl_common(void)
{
	uint32_t reg;

	reg = rd(bus.padctl, XUSB_PADCTL_ELPG_PROGRAM1);
	reg &= ~ELPG_PROGRAM1_AUX_MUX_LP0_CLAMP_EN;
	wr(bus.padctl, XUSB_PADCTL_ELPG_PROGRAM1, reg);
	DELAY(100);

	reg = rd(bus.padctl, XUSB_PADCTL_ELPG_PROGRAM1);
	reg &= ~ELPG_PROGRAM1_AUX_MUX_LP0_CLAMP_EN_EARLY;
	wr(bus.padctl, XUSB_PADCTL_ELPG_PROGRAM1, reg);
	DELAY(100);

	reg = rd(bus.padctl, XUSB_PADCTL_ELPG_PROGRAM1);
	reg &= ~ELPG_PROGRAM1_AUX_MUX_LP0_VCORE_DOWN;
	wr(bus.padctl, XUSB_PADCTL_ELPG_PROGRAM1, reg);
	DELAY(100);
}

/*
 * config_lane() with the "xusb" function, then usb2_enable() for a host port.
 * The trk clock gate and the vbus supply are the caller's.
 */
int
tegra210_xusb_padctl_usb2_port(int idx)
{
	uint32_t reg;

	if (idx < 0 || idx > 3)
		return (EINVAL);

	reg = rd(bus.padctl, XUSB_PADCTL_USB2_PAD_MUX);
	reg &= ~(USB2_LANE_MUX_MASK << USB2_LANE_MUX_SHIFT(idx));
	reg |= USB2_LANE_MUX_XUSB << USB2_LANE_MUX_SHIFT(idx);
	wr(bus.padctl, XUSB_PADCTL_USB2_PAD_MUX, reg);

	reg = rd(bus.padctl, XUSB_PADCTL_USB2_BIAS_PAD_CTL0);
	reg &= ~USB2_BIAS_PAD_CTL0_HS_SQUELCH_LEVEL(~0);
	reg &= ~USB2_BIAS_PAD_CTL0_HS_DISCON_LEVEL(~0);
	reg |= USB2_BIAS_PAD_CTL0_HS_DISCON_LEVEL(0x7);
	wr(bus.padctl, XUSB_PADCTL_USB2_BIAS_PAD_CTL0, reg);

	reg = rd(bus.padctl, XUSB_PADCTL_USB2_PORT_CAP);
	reg &= ~USB2_PORT_CAP_PORT_CAP(idx, ~0);
	reg |= USB2_PORT_CAP_PORT_CAP(idx, USB2_PORT_CAP_PORT_CAP_HOST);
	wr(bus.padctl, XUSB_PADCTL_USB2_PORT_CAP, reg);

	reg = rd(bus.padctl, XUSB_PADCTL_USB2_OTG_PAD_CTL0(idx));
	reg &= ~USB2_OTG_PAD_CTL0_HS_CURR_LEVEL(~0);
	reg &= ~USB2_OTG_PAD_CTL0_HS_SLEW(~0);
	reg &= ~USB2_OTG_PAD_CTL0_PD;
	reg &= ~USB2_OTG_PAD_CTL0_PD2;
	reg &= ~USB2_OTG_PAD_CTL0_PD_ZI;
	reg |= USB2_OTG_PAD_CTL0_HS_SLEW(14);
	reg |= USB2_OTG_PAD_CTL0_HS_CURR_LEVEL(cal.hs_curr_level[idx] +
	    cal.hs_curr_level_offs);
	wr(bus.padctl, XUSB_PADCTL_USB2_OTG_PAD_CTL0(idx), reg);

	reg = rd(bus.padctl, XUSB_PADCTL_USB2_OTG_PAD_CTL1(idx));
	reg &= ~USB2_OTG_PAD_CTL1_TERM_RANGE_ADJ(~0);
	reg &= ~USB2_OTG_PAD_CTL1_RPD_CTRL(~0);
	reg &= ~USB2_OTG_PAD_CTL1_PD_DR;
	reg &= ~USB2_OTG_PAD_CTL1_PD_CHRP_OVRD;
	reg &= ~USB2_OTG_PAD_CTL1_PD_DISC_OVRD;
	reg |= USB2_OTG_PAD_CTL1_TERM_RANGE_ADJ(cal.hs_term_range_adj);
	reg |= USB2_OTG_PAD_CTL1_RPD_CTRL(cal.rpd_ctrl);
	wr(bus.padctl, XUSB_PADCTL_USB2_OTG_PAD_CTL1(idx), reg);

	reg = rd(bus.padctl, XUSB_PADCTL_USB2_BATTERY_CHRG_OTGPAD_CTL1(idx));
	reg &= ~USB2_BATTERY_CHRG_OTGPAD_CTL1_VREG_LEV(~0);
	reg |= USB2_BATTERY_CHRG_OTGPAD_CTL1_VREG_FIX18;
	wr(bus.padctl, XUSB_PADCTL_USB2_BATTERY_CHRG_OTGPAD_CTL1(idx), reg);

	reg = rd(bus.padctl, XUSB_PADCTL_USB2_BIAS_PAD_CTL1);
	reg &= ~USB2_BIAS_PAD_CTL1_TRK_START_TIMER(~0);
	reg &= ~USB2_BIAS_PAD_CTL1_TRK_DONE_RESET_TIMER(~0);
	reg |= USB2_BIAS_PAD_CTL1_TRK_START_TIMER(0x1e);
	reg |= USB2_BIAS_PAD_CTL1_TRK_DONE_RESET_TIMER(0x0a);
	wr(bus.padctl, XUSB_PADCTL_USB2_BIAS_PAD_CTL1, reg);

	reg = rd(bus.padctl, XUSB_PADCTL_USB2_BIAS_PAD_CTL0);
	reg &= ~USB2_BIAS_PAD_CTL0_PD;
	wr(bus.padctl, XUSB_PADCTL_USB2_BIAS_PAD_CTL0, reg);

	/* PureDarwin: power up the tracking circuit, PD_TRK 0 (tegra x1 trm 22.8.10 step 3) */
	DELAY(1);
	reg = rd(bus.padctl, XUSB_PADCTL_USB2_BIAS_PAD_CTL1);
	reg &= ~USB2_BIAS_PAD_CTL1_PD_TRK;
	wr(bus.padctl, XUSB_PADCTL_USB2_BIAS_PAD_CTL1, reg);
	DELAY(100);
	return (0);
}

/* -------------------------------------------------------------------------
 * tegra_xhci.c
 */
#define	T_XUSB_CFG_1				0x004
#define	 CFG_1_BUS_MASTER				(1 << 2)
#define	 CFG_1_MEMORY_SPACE				(1 << 1)
#define	 CFG_1_IO_SPACE					(1 << 0)
#define	T_XUSB_CFG_4				0x010
#define	 CFG_4_BASE_ADDRESS(x)				(((x) & 0x1FFFF) << 15)
#define	T_XUSB_CFG_5				0x014
#define	T_XUSB_CFG_ARU_MAILBOX_CMD		0x0E4
#define  ARU_MAILBOX_CMD_INT_EN				(1U << 31)
#define  ARU_MAILBOX_CMD_DEST_XHCI			(1  << 30)
#define  ARU_MAILBOX_CMD_DEST_SMI			(1  << 29)
#define  ARU_MAILBOX_CMD_DEST_PME			(1  << 28)
#define  ARU_MAILBOX_CMD_DEST_FALC			(1  << 27)
#define	T_XUSB_CFG_ARU_MAILBOX_DATA_IN		0x0E8
#define	 ARU_MAILBOX_DATA_IN_DATA(x)			(((x) & 0xFFFFFF) <<  0)
#define	 ARU_MAILBOX_DATA_IN_TYPE(x)			(((x) & 0x0000FF) << 24)
#define	T_XUSB_CFG_ARU_MAILBOX_DATA_OUT		0x0EC
#define	 ARU_MAILBOX_DATA_OUT_DATA(x)			(((x) >>  0) & 0xFFFFFF)
#define	 ARU_MAILBOX_DATA_OUT_TYPE(x)			(((x) >> 24) & 0x0000FF)
#define	T_XUSB_CFG_ARU_MAILBOX_OWNER		0x0F0
#define	 ARU_MAILBOX_OWNER_SW				2
#define	 ARU_MAILBOX_OWNER_FW				1
#define	 ARU_MAILBOX_OWNER_NONE				0

#define	XUSB_CFG_ARU_C11_CSBRANGE		0x41C	/* ! UNDOCUMENTED ! */
#define	 ARU_C11_CSBRANGE_PAGE(x)			((x) >> 9)
#define	 ARU_C11_CSBRANGE_ADDR(x)			(0x800 + ((x) & 0x1FF))
#define	XUSB_CFG_ARU_SMI_INTR			0x428	/* ! UNDOCUMENTED ! */
#define  ARU_SMI_INTR_EN				(1 << 3)
#define  ARU_SMI_INTR_FW_HANG				(1 << 1)
#define	XUSB_CFG_ARU_RST			0x42C	/* ! UNDOCUMENTED ! */
#define	 ARU_RST_RESET					(1 << 0)

#define	XUSB_HOST_CONFIGURATION			0x180
#define	 CONFIGURATION_EN_FPCI				(1 <<  0)
#define	XUSB_HOST_INTR_MASK			0x188
#define	 INTR_IP_INT_MASK				(1 << 16)
#define	XUSB_HOST_CLKGATE_HYSTERESIS		0x1BC

#define	XUSB_FALCON_CPUCTL			0x100
#define	 CPUCTL_STOPPED					(1 << 5)
#define	 CPUCTL_STARTCPU				(1 << 1)
#define	XUSB_FALCON_BOOTVEC			0x104
#define	XUSB_FALCON_DMACTL			0x10C
#define	XUSB_FALCON_IMFILLRNG1			0x154
#define	 IMFILLRNG1_TAG_HI(x)				(((x) & 0xFFF) << 16)
#define	 IMFILLRNG1_TAG_LO(x)				(((x) & 0xFFF) <<  0)
#define	XUSB_FALCON_IMFILLCTL			0x158

#define	XUSB_CSB_MEMPOOL_APMAP			0x10181C
#define	 APMAP_BOOTPATH					(1U << 31)
#define	XUSB_CSB_MEMPOOL_ILOAD_ATTR		0x101A00
#define	XUSB_CSB_MEMPOOL_ILOAD_BASE_LO		0x101A04
#define	XUSB_CSB_MEMPOOL_ILOAD_BASE_HI		0x101A08
#define	XUSB_CSB_MEMPOOL_L2IMEMOP_SIZE		0x101A10
#define	 L2IMEMOP_SIZE_OFFSET(x)			(((x) & 0x3FF) <<  8)
#define	 L2IMEMOP_SIZE_SIZE(x)				(((x) & 0x0FF) << 24)
#define	XUSB_CSB_MEMPOOL_L2IMEMOP_TRIG		0x101A14
#define	 L2IMEMOP_INVALIDATE_ALL			(0x40 << 24)
#define	 L2IMEMOP_LOAD_LOCKED_RESULT			(0x11 << 24)
#define	XUSB_CSB_MEMPOOL_L2IMEMOP_RESULT        0x101A18
#define	 L2IMEMOP_RESULT_VLD       (1U << 31)

#define XUSB_CSB_IMEM_BLOCK_SIZE	256

/* MBOX commands. */
#define	MBOX_CMD_MSG_ENABLED			 1
#define	MBOX_CMD_INC_FALC_CLOCK			 2
#define	MBOX_CMD_DEC_FALC_CLOCK			 3
#define	MBOX_CMD_INC_SSPI_CLOCK			 4
#define	MBOX_CMD_DEC_SSPI_CLOCK			 5
#define	MBOX_CMD_SET_BW				 6
#define	MBOX_CMD_SET_SS_PWR_GATING		 7
#define	MBOX_CMD_SET_SS_PWR_UNGATING		 8
#define	MBOX_CMD_SAVE_DFE_CTLE_CTX		 9
#define	MBOX_CMD_AIRPLANE_MODE_ENABLED		10
#define	MBOX_CMD_AIRPLANE_MODE_DISABLED		11
#define	MBOX_CMD_START_HSIC_IDLE		12
#define	MBOX_CMD_STOP_HSIC_IDLE			13
#define	MBOX_CMD_DBC_WAKE_STACK			14
#define	MBOX_CMD_HSIC_PRETEND_CONNECT		15
#define	MBOX_CMD_RESET_SSPI			16
#define	MBOX_CMD_DISABLE_SS_LFPS_DETECTION	17
#define	MBOX_CMD_ENABLE_SS_LFPS_DETECTION	18

/* MBOX responses. */
#define	MBOX_CMD_ACK				(0x80 + 0)
#define	MBOX_CMD_NAK				(0x80 + 1)

#define	IPFS_WR4(_r, _v)	wr(bus.ipfs, (_r), (_v))
#define	IPFS_RD4(_r)		rd(bus.ipfs, (_r))
#define	FPCI_WR4(_r, _v)	wr(bus.fpci, (_r), (_v))
#define	FPCI_RD4(_r)		rd(bus.fpci, (_r))

static uint32_t
CSB_RD4(uint32_t addr)
{

	FPCI_WR4(XUSB_CFG_ARU_C11_CSBRANGE, ARU_C11_CSBRANGE_PAGE(addr));
	return (FPCI_RD4(ARU_C11_CSBRANGE_ADDR(addr)));
}

static void
CSB_WR4(uint32_t addr, uint32_t val)
{

	FPCI_WR4(XUSB_CFG_ARU_C11_CSBRANGE, ARU_C11_CSBRANGE_PAGE(addr));
	FPCI_WR4(ARU_C11_CSBRANGE_ADDR(addr), val);
}

uint32_t
tegra210_xusb_falcon_cpuctl(void)
{
	return (CSB_RD4(XUSB_FALCON_CPUCTL));
}

/* init_hw() up to load_fw() */
void
tegra210_xusb_init_ipfs(uint64_t base_addr)
{
	uint32_t reg;

	/* Enable FPCI access */
	reg = IPFS_RD4(XUSB_HOST_CONFIGURATION);
	reg |= CONFIGURATION_EN_FPCI;
	IPFS_WR4(XUSB_HOST_CONFIGURATION, reg);
	IPFS_RD4(XUSB_HOST_CONFIGURATION);

	/* Program bar for XHCI base address */
	reg = FPCI_RD4(T_XUSB_CFG_4);
	reg &= ~CFG_4_BASE_ADDRESS(~0);
	reg |= CFG_4_BASE_ADDRESS((uint32_t)base_addr >> 15);
	FPCI_WR4(T_XUSB_CFG_4, reg);
	FPCI_WR4(T_XUSB_CFG_5, (uint32_t)((uint64_t)(base_addr) >> 32));

	/* Enable bus master */
	reg = FPCI_RD4(T_XUSB_CFG_1);
	reg |= CFG_1_IO_SPACE;
	reg |= CFG_1_MEMORY_SPACE;
	reg |= CFG_1_BUS_MASTER;
	FPCI_WR4(T_XUSB_CFG_1, reg);

	/* Enable Interrupts */
	reg = IPFS_RD4(XUSB_HOST_INTR_MASK);
	reg |= INTR_IP_INT_MASK;
	IPFS_WR4(XUSB_HOST_INTR_MASK, reg);

	/* Set hysteresis */
	IPFS_WR4(XUSB_HOST_CLKGATE_HYSTERESIS, 128);
	LOG("ipfs config 0x%08x intr_mask 0x%08x, fpci cfg1 0x%08x cfg4 0x%08x\n",
	    IPFS_RD4(XUSB_HOST_CONFIGURATION), IPFS_RD4(XUSB_HOST_INTR_MASK),
	    FPCI_RD4(T_XUSB_CFG_1), FPCI_RD4(T_XUSB_CFG_4));
}

/* load_fw(), first half: reset ARU, then whether the falcon already runs */
int
tegra210_xusb_fw_loaded(void)
{
	/* Reset ARU */
	FPCI_WR4(XUSB_CFG_ARU_RST, ARU_RST_RESET);
	DELAY(3000);

	/* Check if FALCON already runs */
	if (CSB_RD4(XUSB_CSB_MEMPOOL_ILOAD_BASE_LO) != 0) {
		LOG("XUSB CPU is already loaded, CPUCTL: 0x%08X\n",
		    CSB_RD4(XUSB_FALCON_CPUCTL));
		return (1);
	}
	return (0);
}

/*
 * load_fw(), second half. The caller has copied the whole image to
 * physically contiguous memory and cleaned it out of the cpu caches.
 */
int
tegra210_xusb_load_fw(const void *fw_vaddr, uint64_t fw_paddr)
{
	const struct tegra_xusb_fw_hdr *fw_hdr;
	uint64_t fw_base;
	uint32_t code_tags, code_size, fw_size;
	int i;

	fw_hdr = (const struct tegra_xusb_fw_hdr *)fw_vaddr;
	fw_size = fw_hdr->fwimg_len;

	/* Setup firmware physical address and size. */
	fw_base = fw_paddr + sizeof(*fw_hdr);
	CSB_WR4(XUSB_CSB_MEMPOOL_ILOAD_ATTR, fw_size);
	CSB_WR4(XUSB_CSB_MEMPOOL_ILOAD_BASE_LO, fw_base & 0xFFFFFFFF);
	CSB_WR4(XUSB_CSB_MEMPOOL_ILOAD_BASE_HI, (uint64_t)fw_base >> 32);
	CSB_WR4(XUSB_CSB_MEMPOOL_APMAP, APMAP_BOOTPATH);

	/* Invalidate full L2IMEM context. */
	CSB_WR4(XUSB_CSB_MEMPOOL_L2IMEMOP_TRIG,
	    L2IMEMOP_INVALIDATE_ALL);

	/* Program load of L2IMEM by boot code. */
	code_tags = howmany(fw_hdr->boot_codetag, XUSB_CSB_IMEM_BLOCK_SIZE);
	code_size = howmany(fw_hdr->boot_codesize, XUSB_CSB_IMEM_BLOCK_SIZE);
	CSB_WR4(XUSB_CSB_MEMPOOL_L2IMEMOP_SIZE,
	    L2IMEMOP_SIZE_OFFSET(code_tags) |
	    L2IMEMOP_SIZE_SIZE(code_size));

	/* Execute L2IMEM boot code fetch. */
	CSB_WR4(XUSB_CSB_MEMPOOL_L2IMEMOP_TRIG,
	    L2IMEMOP_LOAD_LOCKED_RESULT);

	/* Program FALCON auto-fill range and block count */
	CSB_WR4(XUSB_FALCON_IMFILLCTL, code_size);
	CSB_WR4(XUSB_FALCON_IMFILLRNG1,
	    IMFILLRNG1_TAG_LO(code_tags) |
	    IMFILLRNG1_TAG_HI(code_tags + code_size));

	CSB_WR4(XUSB_FALCON_DMACTL, 0);
	/* Wait for CPU */
	for (i = 500; i > 0; i--) {
		if (CSB_RD4(XUSB_CSB_MEMPOOL_L2IMEMOP_RESULT) &
		     L2IMEMOP_RESULT_VLD)
			break;
		DELAY(100);
	}
	if (i <= 0) {
		LOG("Timedout while waiting for DMA, state: 0x%08X\n",
		    CSB_RD4(XUSB_CSB_MEMPOOL_L2IMEMOP_RESULT));
		return (ETIMEDOUT);
	}

	/* Boot FALCON cpu */
	CSB_WR4(XUSB_FALCON_BOOTVEC, fw_hdr->boot_codetag);
	CSB_WR4(XUSB_FALCON_CPUCTL, CPUCTL_STARTCPU);

	/* Wait for CPU */
	for (i = 50; i > 0; i--) {
		if (CSB_RD4(XUSB_FALCON_CPUCTL) == CPUCTL_STOPPED)
			break;
		DELAY(100);
	}
	if (i <= 0) {
		LOG("Timedout while waiting for FALCON cpu, state: 0x%08X\n",
		    CSB_RD4(XUSB_FALCON_CPUCTL));
		return (ETIMEDOUT);
	}

	LOG("Falcon firmware version: %02X.%02X.%04X, created %u\n",
	    (fw_hdr->version_id >> 24) & 0xFF, (fw_hdr->version_id >> 15) & 0xFF,
	    fw_hdr->version_id & 0xFFFF, fw_hdr->fwimg_created_time);
	return (0);
}

/* Respond by ACK/NAK back to FW */
static void
mbox_send_ack(uint32_t cmd, uint32_t data)
{
	uint32_t reg;

	reg = ARU_MAILBOX_DATA_IN_TYPE(cmd) | ARU_MAILBOX_DATA_IN_DATA(data);
	FPCI_WR4(T_XUSB_CFG_ARU_MAILBOX_DATA_IN, reg);

	reg = FPCI_RD4(T_XUSB_CFG_ARU_MAILBOX_CMD);
	reg |= ARU_MAILBOX_CMD_DEST_FALC | ARU_MAILBOX_CMD_INT_EN;
	FPCI_WR4(T_XUSB_CFG_ARU_MAILBOX_CMD, reg);
}

/* Sent command to FW */
int
tegra210_xusb_mbox_send_cmd(uint32_t cmd, uint32_t data)
{
	uint32_t reg;
	int i;

	reg = FPCI_RD4(T_XUSB_CFG_ARU_MAILBOX_OWNER);
	if (reg != ARU_MAILBOX_OWNER_NONE) {
		LOG("CPU mailbox is busy: 0x%08X\n", reg);
		return (EBUSY);
	}
	/* XXX Is this right? Retry loop? Wait before send? */
	FPCI_WR4(T_XUSB_CFG_ARU_MAILBOX_OWNER, ARU_MAILBOX_OWNER_SW);
	reg = FPCI_RD4(T_XUSB_CFG_ARU_MAILBOX_OWNER);
	if (reg != ARU_MAILBOX_OWNER_SW) {
		LOG("Cannot acquire CPU mailbox: 0x%08X\n", reg);
		return (EBUSY);
	}
	reg = ARU_MAILBOX_DATA_IN_TYPE(cmd) | ARU_MAILBOX_DATA_IN_DATA(data);
	FPCI_WR4(T_XUSB_CFG_ARU_MAILBOX_DATA_IN, reg);

	reg = FPCI_RD4(T_XUSB_CFG_ARU_MAILBOX_CMD);
	reg |= ARU_MAILBOX_CMD_DEST_FALC | ARU_MAILBOX_CMD_INT_EN;
	FPCI_WR4(T_XUSB_CFG_ARU_MAILBOX_CMD, reg);

	for (i = 250; i > 0; i--) {
		reg = FPCI_RD4(T_XUSB_CFG_ARU_MAILBOX_OWNER);
		if (reg == ARU_MAILBOX_OWNER_NONE)
			break;
		DELAY(100);
	}
	if (i <= 0) {
		LOG("Command response timeout: 0x%08X\n", reg);
		return (ETIMEDOUT);
	}

	return(0);
}

/*
 * process_msg(). PureDarwin keeps the falcon and ss clocks where
 * tegra210_xusb_setup_clocks() put them and acks a change request with the
 * rate requested, the other replies are FreeBSD's.
 */
static void
process_msg(uint32_t req_cmd, uint32_t req_data,
    uint32_t *resp_cmd, uint32_t *resp_data)
{

	/* In most cases, data are echoed back. */
	*resp_data = req_data;
	switch (req_cmd) {
	case MBOX_CMD_INC_FALC_CLOCK:
	case MBOX_CMD_DEC_FALC_CLOCK:
	case MBOX_CMD_INC_SSPI_CLOCK:
	case MBOX_CMD_DEC_SSPI_CLOCK:
		*resp_cmd = MBOX_CMD_ACK;
		break;

	case MBOX_CMD_SET_BW:
		/* No respense is expected. */
		*resp_cmd = 0;
		break;

	case MBOX_CMD_SET_SS_PWR_GATING:
	case MBOX_CMD_SET_SS_PWR_UNGATING:
		*resp_cmd = MBOX_CMD_NAK;
		break;

	case MBOX_CMD_SAVE_DFE_CTLE_CTX:
		/* Not implemented yet. */
		*resp_cmd = MBOX_CMD_ACK;
		break;

	case MBOX_CMD_START_HSIC_IDLE:
	case MBOX_CMD_STOP_HSIC_IDLE:
		/* Not implemented yet. */
		*resp_cmd = MBOX_CMD_NAK;
		break;

	case MBOX_CMD_DISABLE_SS_LFPS_DETECTION:
	case MBOX_CMD_ENABLE_SS_LFPS_DETECTION:
		/* Not implemented yet. */
		*resp_cmd = MBOX_CMD_NAK;
		break;

	case MBOX_CMD_AIRPLANE_MODE_ENABLED:
	case MBOX_CMD_AIRPLANE_MODE_DISABLED:
	case MBOX_CMD_DBC_WAKE_STACK:
	case MBOX_CMD_HSIC_PRETEND_CONNECT:
	case MBOX_CMD_RESET_SSPI:
		LOG("Received unused/unexpected command: %u\n", req_cmd);
		*resp_cmd = 0;
		break;

	default:
		LOG("Received unknown command: %u\n", req_cmd);
	}
}

/*
 * intr_mbox(), called by polling: PureDarwin runs it while the mailbox
 * command still targets the smi, which intr_mbox() clears when done.
 * Returns the message type, or -1 when nothing is pending.
 */
int
tegra210_xusb_mbox_poll(void)
{
	uint32_t reg, msg, resp_cmd, resp_data;

	if ((FPCI_RD4(T_XUSB_CFG_ARU_MAILBOX_CMD) & ARU_MAILBOX_CMD_DEST_SMI) == 0)
		return (-1);

	/* Clear interrupt first */
	reg = FPCI_RD4(XUSB_CFG_ARU_SMI_INTR);
	FPCI_WR4(XUSB_CFG_ARU_SMI_INTR, reg);
	if (reg & ARU_SMI_INTR_FW_HANG) {
		LOG("XUSB CPU firmware hang!!! CPUCTL: 0x%08X\n",
		    CSB_RD4(XUSB_FALCON_CPUCTL));
	}

	msg = FPCI_RD4(T_XUSB_CFG_ARU_MAILBOX_DATA_OUT);
	resp_cmd = 0;
	process_msg(ARU_MAILBOX_DATA_OUT_TYPE(msg),
	   ARU_MAILBOX_DATA_OUT_DATA(msg), &resp_cmd, &resp_data);
	if (resp_cmd != 0)
		mbox_send_ack(resp_cmd, resp_data);
	else
		FPCI_WR4(T_XUSB_CFG_ARU_MAILBOX_OWNER,
		    ARU_MAILBOX_OWNER_NONE);

	reg = FPCI_RD4(T_XUSB_CFG_ARU_MAILBOX_CMD);
	reg &= ~ARU_MAILBOX_CMD_DEST_SMI;
	FPCI_WR4(T_XUSB_CFG_ARU_MAILBOX_CMD, reg);
	return ((int)ARU_MAILBOX_DATA_OUT_TYPE(msg));
}

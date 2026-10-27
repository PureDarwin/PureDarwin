/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright 2020 Michal Meloun <mmel@FreeBSD.org>
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
 * PureDarwin: the interface of tegra210_gpu.c, excerpts of FreeBSD's
 * Tegra210 CAR, PMC and efuse drivers (freebsd-src e6fb3b51) with the
 * clk/hwreset framework replaced by direct calls.
 */

#ifndef _TEGRA210_GPU_H_
#define	_TEGRA210_GPU_H_

#include <sys/types.h>

/* Peripheral clock and reset indexes, from tegra210_clk_per.c. */
#define	TEGRA210_L(n)	((0 * 32) + (n))
#define	TEGRA210_H(n)	((1 * 32) + (n))
#define	TEGRA210_X(n)	((5 * 32) + (n))

#define	TEGRA210_CLK_PWM	TEGRA210_L(17)
#define	TEGRA210_CLK_I2C5	TEGRA210_H(15)
#define	TEGRA210_CLK_FUSE	TEGRA210_H(7)
#define	TEGRA210_CLK_GPU	TEGRA210_X(24)
#define	TEGRA210_CLK_PLLG_REF	TEGRA210_X(29)

struct tegra210_gpu_bus {
	volatile uint32_t	*car;		/* CAR at 0x60006000 */
	volatile uint32_t	*fuse;		/* efuse block, NULL when unmapped */
	uint32_t		(*pmc_read)(uint32_t off);
	void			(*pmc_write)(uint32_t off, uint32_t val);
};

struct tegra210_sku {
	uint32_t	sku_id;
	int		speedo_rev;
	uint32_t	gpu_speedo_id;
	uint32_t	gpu_speedo_value;
	uint32_t	gpu_iddq_value;
	uint32_t	gpu_process_id;
};

void	tegra210_gpu_attach(const struct tegra210_gpu_bus *bus);

uint64_t tegra210_osc_freq(void);
uint64_t tegra210_osc_div_freq(void);
uint64_t tegra210_clk_m_freq(void);

int	tegra210_pgate_set_gate(uint32_t idx, bool enable);
int	tegra210_pgate_get_gate(uint32_t idx, bool *enabled);
int	tegra210_hwreset_by_idx(uint32_t idx, bool reset);
int	tegra210_hwreset_is_asserted(uint32_t idx);

uint64_t tegra210_pllp_out5_get_freq(uint64_t fin);
int	tegra210_pllp_out5_set_freq(uint64_t fin, uint64_t fout);
int	tegra210_pllp_out5_set_gate(bool enable);

int	tegra210_efuse_sku(struct tegra210_sku *sku);

#endif /* _TEGRA210_GPU_H_ */

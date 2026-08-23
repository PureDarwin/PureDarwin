/* SPDX-License-Identifier: GPL-2.0-or-later OR MIT
 *
 * Copyright (c) 2026 PureDarwin Project
 *
 * BCM2835 Arasan EMMC controller bring-up.
 *
 * Everything needed to get the controller from cold to "ready to take SD
 * commands": its clock, the GPIO routing that connects the card to it, and the
 * host reset. SD protocol itself is deliberately out of scope and stays in the
 * caller.
 *
 * Written from BCM2835-ARM-Peripherals.pdf section 5 (External Mass Media
 * Controller) and the published CPRMAN register map.
 *
 * The caller maps the three register blocks and supplies accessors, so this
 * file needs no knowledge of address spaces, MMU state or mapping APIs.
 */
#ifndef BCM2835_EMMC_BRINGUP_H
#define BCM2835_EMMC_BRINGUP_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Register blocks the caller must map. Sizes are minimums. */
enum bcm2835_emmc_block {
	BCM2835_EMMC_BLOCK_EMMC = 0,    /* bus 0x7E300000, 0x100 bytes  */
	BCM2835_EMMC_BLOCK_GPIO,        /* bus 0x7E200000, 0xa0 bytes   */
	BCM2835_EMMC_BLOCK_CPRMAN,      /* bus 0x7E101000, 0x200 bytes  */
};

struct bcm2835_emmc_ops {
	uint32_t (*read32)(void *ctx, enum bcm2835_emmc_block blk, uint32_t off);
	void     (*write32)(void *ctx, enum bcm2835_emmc_block blk, uint32_t off,
	                    uint32_t val);
	void     (*udelay)(void *ctx, uint32_t us);
	void     (*log)(void *ctx, const char *fmt, ...);   /* optional, may be NULL */
	void     *ctx;
};

enum {
	BCM2835_EMMC_OK        =  0,
	BCM2835_EMMC_ERR_ARGS  = -1,
	BCM2835_EMMC_ERR_CLOCK = -2,
	BCM2835_EMMC_ERR_RESET = -3,
};

enum bcm2835_emmc_clock_src {
	BCM2835_EMMC_CLOCK_OSC = 0,
	BCM2835_EMMC_CLOCK_PLLC,
};

/*
 * Program the EMMC clock. src_hz is ignored for the crystal and required for
 * PLLC. The rate actually programmed is returned in out_hz, which the caller
 * needs in order to compute the SDHCI clock divisors.
 */
int bcm2835_emmc_enable_clock(const struct bcm2835_emmc_ops *ops,
                              enum bcm2835_emmc_clock_src src,
                              uint32_t src_hz, uint32_t *out_hz);

/*
 * Connect GPIO 48-53 to the EMMC controller (ALT3) with pull-ups. These pins
 * carry the card to one controller at a time, so this takes ownership of it.
 */
int bcm2835_emmc_route_pins(const struct bcm2835_emmc_ops *ops);

/* Full host reset, leaving interrupt flags recorded but not routed to a CPU. */
int bcm2835_emmc_reset_host(const struct bcm2835_emmc_ops *ops);

/* Clock, then pins, then reset. */
int bcm2835_emmc_bringup(const struct bcm2835_emmc_ops *ops,
                         enum bcm2835_emmc_clock_src src,
                         uint32_t src_hz, uint32_t *out_hz);

/* Reported by the controller's SLOTISR_VER register; useful in caller logs. */
uint32_t bcm2835_emmc_host_version(const struct bcm2835_emmc_ops *ops);

#ifdef __cplusplus
}
#endif

#endif /* BCM2835_EMMC_BRINGUP_H */

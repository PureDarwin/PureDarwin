/* SPDX-License-Identifier: GPL-2.0-or-later OR MIT
 *
 * Copyright (c) 2026 PureDarwin Project
 *
 * BCM2835 Arasan EMMC controller bring-up. See bcm2835_emmc_bringup.h.
 */

#include "bcm2835_emmc_bringup.h"

#include <stddef.h>

/* EMMC block */
#define EMMC_CONTROL0           0x28
#define EMMC_CONTROL1           0x2c
#define EMMC_INTERRUPT          0x30
#define EMMC_IRPT_MASK          0x34
#define EMMC_IRPT_EN            0x38
#define EMMC_SLOTISR_VER        0xfc

#define C1_SRST_HC              (1u << 24)

/* GPIO block. GPFSEL4 covers pins 40-49 and GPFSEL5 pins 50-59, 3 bits each. */
#define GPIO_GPFSEL4            0x10
#define GPIO_GPFSEL5            0x14
#define GPIO_GPPUD              0x94
#define GPIO_GPPUDCLK1          0x9c
#define GPIO_FSEL_ALT3          7
#define GPIO_PUD_UP             2

/* CPRMAN block, offsets relative to bus 0x7E101000. */
#define CM_EMMCCTL              0x1c0
#define CM_EMMCDIV              0x1c4
#define CM_PASSWORD             0x5a000000u
#define CM_SRC_OSC              1
#define CM_SRC_PLLC             5
#define CM_ENAB                 (1u << 4)
#define CM_KILL                 (1u << 5)
#define CM_BUSY                 (1u << 7)

#define OSC_HZ                  19200000u

/* Polls are bounded so a dead block reports failure instead of hanging. */
#define POLL_ITERATIONS         1000
#define POLL_INTERVAL_US        10
#define RESET_INTERVAL_US       100

static void
emmc_log(const struct bcm2835_emmc_ops *ops, const char *fmt)
{
	if (ops->log != NULL) {
		ops->log(ops->ctx, "%s", fmt);
	}
}

static int
ops_valid(const struct bcm2835_emmc_ops *ops)
{
	return ops != NULL && ops->read32 != NULL && ops->write32 != NULL &&
	       ops->udelay != NULL;
}

uint32_t
bcm2835_emmc_host_version(const struct bcm2835_emmc_ops *ops)
{
	if (!ops_valid(ops)) {
		return 0;
	}
	return ops->read32(ops->ctx, BCM2835_EMMC_BLOCK_EMMC, EMMC_SLOTISR_VER);
}

int
bcm2835_emmc_enable_clock(const struct bcm2835_emmc_ops *ops,
                          enum bcm2835_emmc_clock_src src,
                          uint32_t src_hz, uint32_t *out_hz)
{
	uint32_t sel, rate;
	int i;

	if (!ops_valid(ops)) {
		return BCM2835_EMMC_ERR_ARGS;
	}

	switch (src) {
	case BCM2835_EMMC_CLOCK_OSC:
		sel = CM_SRC_OSC;
		rate = OSC_HZ;
		break;
	case BCM2835_EMMC_CLOCK_PLLC:
		if (src_hz == 0) {
			return BCM2835_EMMC_ERR_ARGS;
		}
		sel = CM_SRC_PLLC;
		rate = src_hz;
		break;
	default:
		return BCM2835_EMMC_ERR_ARGS;
	}

	/* Stop the generator before touching the divider, or the output glitches. */
	ops->write32(ops->ctx, BCM2835_EMMC_BLOCK_CPRMAN, CM_EMMCCTL,
	             CM_PASSWORD | CM_KILL);
	for (i = 0; i < POLL_ITERATIONS; i++) {
		if ((ops->read32(ops->ctx, BCM2835_EMMC_BLOCK_CPRMAN, CM_EMMCCTL) &
		     CM_BUSY) == 0) {
			break;
		}
		ops->udelay(ops->ctx, POLL_INTERVAL_US);
	}

	/* Integer divider of 1: the SDHCI divisors do the rest. */
	ops->write32(ops->ctx, BCM2835_EMMC_BLOCK_CPRMAN, CM_EMMCDIV,
	             CM_PASSWORD | (1u << 12));
	ops->write32(ops->ctx, BCM2835_EMMC_BLOCK_CPRMAN, CM_EMMCCTL,
	             CM_PASSWORD | sel);
	ops->udelay(ops->ctx, POLL_INTERVAL_US);
	ops->write32(ops->ctx, BCM2835_EMMC_BLOCK_CPRMAN, CM_EMMCCTL,
	             CM_PASSWORD | sel | CM_ENAB);

	for (i = 0; i < POLL_ITERATIONS; i++) {
		if ((ops->read32(ops->ctx, BCM2835_EMMC_BLOCK_CPRMAN, CM_EMMCCTL) &
		     CM_BUSY) != 0) {
			break;
		}
		ops->udelay(ops->ctx, POLL_INTERVAL_US);
	}
	if ((ops->read32(ops->ctx, BCM2835_EMMC_BLOCK_CPRMAN, CM_EMMCCTL) &
	     CM_BUSY) == 0) {
		emmc_log(ops, "bcm2835_emmc: clock never started");
		return BCM2835_EMMC_ERR_CLOCK;
	}

	if (out_hz != NULL) {
		*out_hz = rate;
	}
	if (ops->log != NULL) {
		ops->log(ops->ctx, "bcm2835_emmc: clock enabled at %u Hz", rate);
	}
	return BCM2835_EMMC_OK;
}

int
bcm2835_emmc_route_pins(const struct bcm2835_emmc_ops *ops)
{
	uint32_t v;
	int pin;

	if (!ops_valid(ops)) {
		return BCM2835_EMMC_ERR_ARGS;
	}

	v = ops->read32(ops->ctx, BCM2835_EMMC_BLOCK_GPIO, GPIO_GPFSEL4);
	for (pin = 48; pin <= 49; pin++) {
		int sh = (pin - 40) * 3;
		v = (v & ~(7u << sh)) | ((uint32_t)GPIO_FSEL_ALT3 << sh);
	}
	ops->write32(ops->ctx, BCM2835_EMMC_BLOCK_GPIO, GPIO_GPFSEL4, v);

	v = ops->read32(ops->ctx, BCM2835_EMMC_BLOCK_GPIO, GPIO_GPFSEL5);
	for (pin = 50; pin <= 53; pin++) {
		int sh = (pin - 50) * 3;
		v = (v & ~(7u << sh)) | ((uint32_t)GPIO_FSEL_ALT3 << sh);
	}
	ops->write32(ops->ctx, BCM2835_EMMC_BLOCK_GPIO, GPIO_GPFSEL5, v);

	/* Program the pull direction, let it settle, then clock it into the pins. */
	ops->write32(ops->ctx, BCM2835_EMMC_BLOCK_GPIO, GPIO_GPPUD, GPIO_PUD_UP);
	ops->udelay(ops->ctx, POLL_INTERVAL_US);
	ops->write32(ops->ctx, BCM2835_EMMC_BLOCK_GPIO, GPIO_GPPUDCLK1,
	             0x3fu << 16);
	ops->udelay(ops->ctx, POLL_INTERVAL_US);
	ops->write32(ops->ctx, BCM2835_EMMC_BLOCK_GPIO, GPIO_GPPUD, 0);
	ops->write32(ops->ctx, BCM2835_EMMC_BLOCK_GPIO, GPIO_GPPUDCLK1, 0);

	emmc_log(ops, "bcm2835_emmc: GPIO 48-53 routed to the EMMC controller");
	return BCM2835_EMMC_OK;
}

int
bcm2835_emmc_reset_host(const struct bcm2835_emmc_ops *ops)
{
	int i;

	if (!ops_valid(ops)) {
		return BCM2835_EMMC_ERR_ARGS;
	}

	ops->write32(ops->ctx, BCM2835_EMMC_BLOCK_EMMC, EMMC_CONTROL0, 0);
	ops->write32(ops->ctx, BCM2835_EMMC_BLOCK_EMMC, EMMC_CONTROL1, C1_SRST_HC);

	for (i = 0; i < POLL_ITERATIONS; i++) {
		ops->udelay(ops->ctx, RESET_INTERVAL_US);
		if ((ops->read32(ops->ctx, BCM2835_EMMC_BLOCK_EMMC, EMMC_CONTROL1) &
		     C1_SRST_HC) == 0) {
			break;
		}
	}
	if ((ops->read32(ops->ctx, BCM2835_EMMC_BLOCK_EMMC, EMMC_CONTROL1) &
	     C1_SRST_HC) != 0) {
		emmc_log(ops, "bcm2835_emmc: host reset timed out");
		return BCM2835_EMMC_ERR_RESET;
	}

	/*
	 * Unmask all interrupt status sources for polling, but disable CPU interrupt
	 * signalling. Bring-up uses INTERRUPT as a polled status register.
	 */
	ops->write32(ops->ctx, BCM2835_EMMC_BLOCK_EMMC, EMMC_IRPT_EN, 0);
	ops->write32(ops->ctx, BCM2835_EMMC_BLOCK_EMMC, EMMC_IRPT_MASK, 0xffffffffu);
	ops->write32(ops->ctx, BCM2835_EMMC_BLOCK_EMMC, EMMC_INTERRUPT, 0xffffffffu);
	return BCM2835_EMMC_OK;
}

int
bcm2835_emmc_bringup(const struct bcm2835_emmc_ops *ops,
                     enum bcm2835_emmc_clock_src src,
                     uint32_t src_hz, uint32_t *out_hz)
{
	int err;

	err = bcm2835_emmc_enable_clock(ops, src, src_hz, out_hz);
	if (err != BCM2835_EMMC_OK) {
		return err;
	}
	err = bcm2835_emmc_route_pins(ops);
	if (err != BCM2835_EMMC_OK) {
		return err;
	}
	return bcm2835_emmc_reset_host(ops);
}

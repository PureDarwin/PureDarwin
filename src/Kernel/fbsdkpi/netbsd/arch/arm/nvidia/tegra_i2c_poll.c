/* $NetBSD: tegra_i2c.c,v 1.28 2025/09/16 11:55:16 thorpej Exp $ */

/*-
 * Copyright (c) 2015 Jared D. McNeill <jmcneill@invisible.ca>
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * PureDarwin: the polled transfer path of NetBSD's tegra_i2c.c (v1.28) for
 * one controller already set up by the boot firmware. Attach, interrupt
 * handling, controller init and the iic framework are left out. The
 * register map is tegra_i2creg.h (v1.2) unchanged.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/errno.h>

#ifndef __BIT
#define	__BIT(n)		(1U << (n))
#define	__BITS(hi, lo)		((~0U >> (31 - (hi))) & ~((1U << (lo)) - 1))
#define	__LOWEST_SET_BIT(m)	((((m) - 1) & (m)) ^ (m))
#define	__SHIFTOUT(x, m)	(((x) & (m)) / __LOWEST_SET_BIT(m))
#define	__SHIFTIN(x, m)		((x) * __LOWEST_SET_BIT(m))
#endif

#include "tegra_i2creg.h"
#include "tegra_i2c_poll.h"

#define	I2C_READ(sc, reg)	((sc)->regs[(reg) / 4])
#define	I2C_WRITE(sc, reg, val)	do { (sc)->regs[(reg) / 4] = (val); \
	__asm__ volatile ("dsb sy" ::: "memory"); } while (0)
#define	I2C_SET_CLEAR(sc, reg, set, clr) \
	I2C_WRITE((sc), (reg), (I2C_READ((sc), (reg)) | (set)) & ~(clr))
#define	delay(us)		DELAY(us)
#define	uimin(a, b)		((a) < (b) ? (a) : (b))

static int
tegra_i2c_wait(struct tegra_i2c_poll *sc)
{
	int retry;
	uint32_t stat = 0;

	retry = 100000;

	while (--retry > 0) {
		stat = I2C_READ(sc, I2C_INTERRUPT_STATUS_REG);
		if (stat & I2C_INTERRUPT_STATUS_PACKET_XFER_COMPLETE) {
			break;
		}
		delay(10);
	}
	if (retry == 0) {
		printf("tegra_i2c: timed out, status = %#x\n", stat);
		return ETIMEDOUT;
	}

	const uint32_t err_mask =
	    I2C_INTERRUPT_STATUS_NOACK |
	    I2C_INTERRUPT_STATUS_ARB_LOST |
	    I2C_INTERRUPT_MASK_TIMEOUT;

	if (stat & err_mask) {
		printf("tegra_i2c: error, status = %#x\n", stat);
		return EIO;
	}

	return 0;
}

static int
tegra_i2c_write(struct tegra_i2c_poll *sc, uint8_t addr, const uint8_t *buf,
    size_t buflen, bool repeat_start)
{
	const uint8_t *p = buf;
	size_t n, resid = buflen;
	uint32_t data;
	int retry;

	const uint32_t istatus = I2C_READ(sc, I2C_INTERRUPT_STATUS_REG);
	I2C_WRITE(sc, I2C_INTERRUPT_STATUS_REG, istatus);

	/* Generic Header 0 */
	I2C_WRITE(sc, I2C_TX_PACKET_FIFO_REG,
	    __SHIFTIN(I2C_IOPACKET_WORD0_PROTHDRSZ_REQ,
		      I2C_IOPACKET_WORD0_PROTHDRSZ) |
	    __SHIFTIN(sc->cid, I2C_IOPACKET_WORD0_CONTROLLERID) |
	    __SHIFTIN(1, I2C_IOPACKET_WORD0_PKTID) |
	    __SHIFTIN(I2C_IOPACKET_WORD0_PROTOCOL_I2C,
		      I2C_IOPACKET_WORD0_PROTOCOL) |
	    __SHIFTIN(I2C_IOPACKET_WORD0_PKTTYPE_REQ,
		      I2C_IOPACKET_WORD0_PKTTYPE));
	/* Generic Header 1 */
	I2C_WRITE(sc, I2C_TX_PACKET_FIFO_REG,
	    __SHIFTIN(buflen - 1, I2C_IOPACKET_WORD1_PAYLOADSIZE));
	/* I2C Master Transmit Packet Header */
	I2C_WRITE(sc, I2C_TX_PACKET_FIFO_REG,
	    I2C_IOPACKET_XMITHDR_IE |
	    (repeat_start ? I2C_IOPACKET_XMITHDR_REPEAT_STARTSTOP : 0) |
	    __SHIFTIN((addr << 1), I2C_IOPACKET_XMITHDR_SLAVE_ADDR));

	/* Transmit data */
	while (resid > 0) {
		retry = 10000;
		while (--retry > 0) {
			const uint32_t fs = I2C_READ(sc, I2C_FIFO_STATUS_REG);
			const u_int cnt =
			    __SHIFTOUT(fs, I2C_FIFO_STATUS_TX_FIFO_EMPTY_CNT);
			if (cnt > 0)
				break;
			delay(10);
		}
		if (retry == 0) {
			printf("tegra_i2c: TX FIFO timeout\n");
			return ETIMEDOUT;
		}

		for (n = 0, data = 0; n < uimin(resid, 4); n++) {
			data |= (uint32_t)p[n] << (n * 8);
		}
		I2C_WRITE(sc, I2C_TX_PACKET_FIFO_REG, data);
		p += uimin(resid, 4);
		resid -= uimin(resid, 4);
	}

	return tegra_i2c_wait(sc);
}

static int
tegra_i2c_read(struct tegra_i2c_poll *sc, uint8_t addr, uint8_t *buf,
    size_t buflen)
{
	uint8_t *p = buf;
	size_t n, resid = buflen;
	uint32_t data;
	int retry;

	const uint32_t istatus = I2C_READ(sc, I2C_INTERRUPT_STATUS_REG);
	I2C_WRITE(sc, I2C_INTERRUPT_STATUS_REG, istatus);

	/* Generic Header 0 */
	I2C_WRITE(sc, I2C_TX_PACKET_FIFO_REG,
	    __SHIFTIN(I2C_IOPACKET_WORD0_PROTHDRSZ_REQ,
		      I2C_IOPACKET_WORD0_PROTHDRSZ) |
	    __SHIFTIN(sc->cid, I2C_IOPACKET_WORD0_CONTROLLERID) |
	    __SHIFTIN(1, I2C_IOPACKET_WORD0_PKTID) |
	    __SHIFTIN(I2C_IOPACKET_WORD0_PROTOCOL_I2C,
		      I2C_IOPACKET_WORD0_PROTOCOL) |
	    __SHIFTIN(I2C_IOPACKET_WORD0_PKTTYPE_REQ,
		      I2C_IOPACKET_WORD0_PKTTYPE));
	/* Generic Header 1 */
	I2C_WRITE(sc, I2C_TX_PACKET_FIFO_REG,
	    __SHIFTIN(buflen - 1, I2C_IOPACKET_WORD1_PAYLOADSIZE));
	/* I2C Master Transmit Packet Header */
	I2C_WRITE(sc, I2C_TX_PACKET_FIFO_REG,
	    I2C_IOPACKET_XMITHDR_IE | I2C_IOPACKET_XMITHDR_READ |
	    __SHIFTIN((addr << 1) | 1, I2C_IOPACKET_XMITHDR_SLAVE_ADDR));

	while (resid > 0) {
		retry = 10000;
		while (--retry > 0) {
			const uint32_t fs = I2C_READ(sc, I2C_FIFO_STATUS_REG);
			const u_int cnt =
			    __SHIFTOUT(fs, I2C_FIFO_STATUS_RX_FIFO_FULL_CNT);
			if (cnt > 0)
				break;
			delay(10);
		}
		if (retry == 0) {
			printf("tegra_i2c: RX FIFO timeout\n");
			return ETIMEDOUT;
		}

		data = I2C_READ(sc, I2C_RX_FIFO_REG);
		for (n = 0; n < uimin(resid, 4); n++) {
			p[n] = (data >> (n * 8)) & 0xff;
		}
		p += uimin(resid, 4);
		resid -= uimin(resid, 4);
	}

	return tegra_i2c_wait(sc);
}

/* tegra_i2c_exec() for one polled operation, without the bus lock. */
int
tegra_i2c_poll_exec(struct tegra_i2c_poll *sc, uint8_t addr,
    const void *cmdbuf, size_t cmdlen, void *buf, size_t buflen, bool read)
{
	int retry, error = 0;

	if (buflen == 0 && cmdlen == 0)
		return EINVAL;

	const uint32_t flush_mask =
	    I2C_FIFO_CONTROL_TX_FIFO_FLUSH | I2C_FIFO_CONTROL_RX_FIFO_FLUSH;

	I2C_SET_CLEAR(sc, I2C_FIFO_CONTROL_REG, flush_mask, 0);
	for (retry = 10000; retry > 0; retry--) {
		const uint32_t v = I2C_READ(sc, I2C_FIFO_CONTROL_REG);
		if ((v & flush_mask) == 0)
			break;
		delay(1);
	}
	if (retry == 0) {
		printf("tegra_i2c: timeout flushing FIFO\n");
		return EIO;
	}

	if (cmdlen > 0) {
		error = tegra_i2c_write(sc, addr, cmdbuf, cmdlen,
		    buflen > 0 ? true : false);
		if (error)
			return error;
	}

	if (buflen > 0) {
		if (read)
			error = tegra_i2c_read(sc, addr, buf, buflen);
		else
			error = tegra_i2c_write(sc, addr, buf, buflen, false);
	}
	return error;
}

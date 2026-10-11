/*
 * Copyright 2008 Advanced Micro Devices, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE COPYRIGHT HOLDER(S) OR AUTHOR(S) BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 *
 * Author: Stanislaw Skowronek
 *
 * Adapted for PureDarwin from the Linux radeon driver (atom.c, atom.h, atom-bits.h).
 */

#include "RadeonAtom.h"

#include <IOKit/IOLib.h>
#include <kern/clock.h>

#define ATOM_BIG_ENDIAN 0
#include "atombios.h"

#define ATOM_COND_ABOVE		0
#define ATOM_COND_ABOVEOREQUAL	1
#define ATOM_COND_ALWAYS	2
#define ATOM_COND_BELOW		3
#define ATOM_COND_BELOWOREQUAL	4
#define ATOM_COND_EQUAL		5
#define ATOM_COND_NOTEQUAL	6

#define ATOM_PORT_ATI	0
#define ATOM_PORT_PCI	1
#define ATOM_PORT_SYSIO	2

#define ATOM_UNIT_MICROSEC	0
#define ATOM_UNIT_MILLISEC	1

typedef struct {
	struct atom_context *ctx;
	uint32_t *ps, *ws;
	int ps_size, ws_size;
	int ps_shift;
	uint16_t start;
	unsigned last_jump;
	uint64_t last_jump_ms;
	bool abort;
} atom_exec_context;

int atom_trace = 0;

static int atom_depth;

#define TRACE(...) do { if (atom_trace) kprintf(__VA_ARGS__); } while (0)

static int atom_execute_table_locked(struct atom_context *ctx, int index, uint32_t *params, int params_size);

static uint32_t atom_arg_mask[8] = {
	0xFFFFFFFF, 0x0000FFFF, 0x00FFFF00, 0xFFFF0000,
	0x000000FF, 0x0000FF00, 0x00FF0000, 0xFF000000
};
static int atom_arg_shift[8] = { 0, 0, 8, 16, 0, 8, 16, 24 };

static int atom_dst_to_src[8][4] = {
	{0, 0, 0, 0},
	{1, 2, 3, 0},
	{1, 2, 3, 0},
	{1, 2, 3, 0},
	{4, 5, 6, 7},
	{4, 5, 6, 7},
	{4, 5, 6, 7},
	{4, 5, 6, 7},
};
static int atom_def_dst[8] = { 0, 0, 1, 2, 0, 1, 2, 3 };

static inline uint8_t get_u8(struct atom_context *c, int ptr)
{
	if (ptr < 0 || (uint32_t)ptr >= c->bios_size)
		return 0;
	return ((unsigned char *)c->bios)[ptr];
}

static inline uint16_t get_u16(struct atom_context *c, int ptr)
{
	return get_u8(c, ptr) | (((uint16_t)get_u8(c, ptr + 1)) << 8);
}

static inline uint32_t get_u32(struct atom_context *c, int ptr)
{
	return get_u16(c, ptr) | (((uint32_t)get_u16(c, ptr + 2)) << 16);
}

#define U8(ptr) get_u8(ctx->ctx, (ptr))
#define CU8(ptr) get_u8(ctx, (ptr))
#define U16(ptr) get_u16(ctx->ctx, (ptr))
#define CU16(ptr) get_u16(ctx, (ptr))
#define U32(ptr) get_u32(ctx->ctx, (ptr))
#define CU32(ptr) get_u32(ctx, (ptr))

static uint64_t now_ms(void)
{
	uint64_t abs, ns;
	clock_get_uptime(&abs);
	absolutetime_to_nanoseconds(abs, &ns);
	return ns / 1000000;
}

static uint32_t atom_iio_execute(struct atom_context *ctx, int base,
				 uint32_t index, uint32_t data)
{
	uint32_t temp = 0xCDCDCDCD;

	while (1)
		switch (CU8(base)) {
		case ATOM_IIO_NOP:
			base++;
			break;
		case ATOM_IIO_READ:
			temp = ctx->card->ioreg_read(ctx->card, CU16(base + 1));
			base += 3;
			break;
		case ATOM_IIO_WRITE:
			ctx->card->ioreg_write(ctx->card, CU16(base + 1), temp);
			base += 3;
			break;
		case ATOM_IIO_CLEAR:
			temp &= ~((0xFFFFFFFF >> (32 - CU8(base + 1))) << CU8(base + 2));
			base += 3;
			break;
		case ATOM_IIO_SET:
			temp |= (0xFFFFFFFF >> (32 - CU8(base + 1))) << CU8(base + 2);
			base += 3;
			break;
		case ATOM_IIO_MOVE_INDEX:
			temp &= ~((0xFFFFFFFF >> (32 - CU8(base + 1))) << CU8(base + 3));
			temp |= ((index >> CU8(base + 2)) & (0xFFFFFFFF >> (32 - CU8(base + 1)))) << CU8(base + 3);
			base += 4;
			break;
		case ATOM_IIO_MOVE_DATA:
			temp &= ~((0xFFFFFFFF >> (32 - CU8(base + 1))) << CU8(base + 3));
			temp |= ((data >> CU8(base + 2)) & (0xFFFFFFFF >> (32 - CU8(base + 1)))) << CU8(base + 3);
			base += 4;
			break;
		case ATOM_IIO_MOVE_ATTR:
			temp &= ~((0xFFFFFFFF >> (32 - CU8(base + 1))) << CU8(base + 3));
			temp |= ((ctx->io_attr >> CU8(base + 2)) & (0xFFFFFFFF >> (32 - CU8(base + 1)))) << CU8(base + 3);
			base += 4;
			break;
		case ATOM_IIO_END:
			return temp;
		default:
			kprintf("atom: unknown iio opcode\n");
			return 0;
		}
}

static uint32_t atom_get_src_int(atom_exec_context *ctx, uint8_t attr,
				 int *ptr, uint32_t *saved)
{
	uint32_t idx, val = 0xCDCDCDCD, align, arg;
	struct atom_context *gctx = ctx->ctx;
	arg = attr & 7;
	align = (attr >> 3) & 7;
	switch (arg) {
	case ATOM_ARG_REG:
		idx = U16(*ptr);
		(*ptr) += 2;
		idx += gctx->reg_block;
		switch (gctx->io_mode) {
		case ATOM_IO_MM:
			val = gctx->card->reg_read(gctx->card, idx);
			break;
		case ATOM_IO_PCI:
			kprintf("atom: pci registers are not implemented\n");
			return 0;
		case ATOM_IO_SYSIO:
			kprintf("atom: sysio registers are not implemented\n");
			return 0;
		default:
			if (!(gctx->io_mode & 0x80)) {
				kprintf("atom: bad io mode\n");
				return 0;
			}
			if (!gctx->iio[gctx->io_mode & 0x7F]) {
				kprintf("atom: undefined indirect io read method %d\n", gctx->io_mode & 0x7F);
				return 0;
			}
			val = atom_iio_execute(gctx, gctx->iio[gctx->io_mode & 0x7F], idx, 0);
		}
		break;
	case ATOM_ARG_PS:
		idx = U8(*ptr);
		(*ptr)++;
		if ((int)idx < ctx->ps_size)
			val = ctx->ps[idx];
		else
			kprintf("atom: ps index out of range: %d > %d\n", idx, ctx->ps_size);
		break;
	case ATOM_ARG_WS:
		idx = U8(*ptr);
		(*ptr)++;
		switch (idx) {
		case ATOM_WS_QUOTIENT:
			val = gctx->divmul[0];
			break;
		case ATOM_WS_REMAINDER:
			val = gctx->divmul[1];
			break;
		case ATOM_WS_DATAPTR:
			val = gctx->data_block;
			break;
		case ATOM_WS_SHIFT:
			val = gctx->shift;
			break;
		case ATOM_WS_OR_MASK:
			val = 1 << gctx->shift;
			break;
		case ATOM_WS_AND_MASK:
			val = ~(1 << gctx->shift);
			break;
		case ATOM_WS_FB_WINDOW:
			val = gctx->fb_base;
			break;
		case ATOM_WS_ATTRIBUTES:
			val = gctx->io_attr;
			break;
		case ATOM_WS_REGPTR:
			val = gctx->reg_block;
			break;
		default:
			if ((int)idx < ctx->ws_size)
				val = ctx->ws[idx];
			else
				kprintf("atom: ws index out of range: %d > %d\n", idx, ctx->ws_size);
		}
		break;
	case ATOM_ARG_ID:
		idx = U16(*ptr);
		(*ptr) += 2;
		val = U32(idx + gctx->data_block);
		break;
	case ATOM_ARG_FB:
		idx = U8(*ptr);
		(*ptr)++;
		if ((gctx->fb_base + (idx * 4) + 4) > (uint32_t)gctx->scratch_size_bytes) {
			kprintf("atom: fb read beyond scratch region: %d vs. %d\n",
				gctx->fb_base + (idx * 4), gctx->scratch_size_bytes);
			val = 0;
		} else
			val = gctx->scratch[(gctx->fb_base / 4) + idx];
		break;
	case ATOM_ARG_IMM:
		switch (align) {
		case ATOM_SRC_DWORD:
			val = U32(*ptr);
			(*ptr) += 4;
			return val;
		case ATOM_SRC_WORD0:
		case ATOM_SRC_WORD8:
		case ATOM_SRC_WORD16:
			val = U16(*ptr);
			(*ptr) += 2;
			return val;
		case ATOM_SRC_BYTE0:
		case ATOM_SRC_BYTE8:
		case ATOM_SRC_BYTE16:
		case ATOM_SRC_BYTE24:
			val = U8(*ptr);
			(*ptr)++;
			return val;
		}
		return 0;
	case ATOM_ARG_PLL:
		idx = U8(*ptr);
		(*ptr)++;
		val = gctx->card->pll_read(gctx->card, idx);
		break;
	case ATOM_ARG_MC:
		idx = U8(*ptr);
		(*ptr)++;
		val = gctx->card->mc_read(gctx->card, idx);
		break;
	}
	if (saved)
		*saved = val;
	val &= atom_arg_mask[align];
	val >>= atom_arg_shift[align];
	return val;
}

static void atom_skip_src_int(atom_exec_context *ctx, uint8_t attr, int *ptr)
{
	uint32_t align = (attr >> 3) & 7, arg = attr & 7;
	switch (arg) {
	case ATOM_ARG_REG:
	case ATOM_ARG_ID:
		(*ptr) += 2;
		break;
	case ATOM_ARG_PLL:
	case ATOM_ARG_MC:
	case ATOM_ARG_PS:
	case ATOM_ARG_WS:
	case ATOM_ARG_FB:
		(*ptr)++;
		break;
	case ATOM_ARG_IMM:
		switch (align) {
		case ATOM_SRC_DWORD:
			(*ptr) += 4;
			return;
		case ATOM_SRC_WORD0:
		case ATOM_SRC_WORD8:
		case ATOM_SRC_WORD16:
			(*ptr) += 2;
			return;
		case ATOM_SRC_BYTE0:
		case ATOM_SRC_BYTE8:
		case ATOM_SRC_BYTE16:
		case ATOM_SRC_BYTE24:
			(*ptr)++;
			return;
		}
		return;
	}
}

static uint32_t atom_get_src(atom_exec_context *ctx, uint8_t attr, int *ptr)
{
	return atom_get_src_int(ctx, attr, ptr, NULL);
}

static uint32_t atom_get_src_direct(atom_exec_context *ctx, uint8_t align, int *ptr)
{
	uint32_t val = 0xCDCDCDCD;

	switch (align) {
	case ATOM_SRC_DWORD:
		val = U32(*ptr);
		(*ptr) += 4;
		break;
	case ATOM_SRC_WORD0:
	case ATOM_SRC_WORD8:
	case ATOM_SRC_WORD16:
		val = U16(*ptr);
		(*ptr) += 2;
		break;
	case ATOM_SRC_BYTE0:
	case ATOM_SRC_BYTE8:
	case ATOM_SRC_BYTE16:
	case ATOM_SRC_BYTE24:
		val = U8(*ptr);
		(*ptr)++;
		break;
	}
	return val;
}

static uint32_t atom_get_dst(atom_exec_context *ctx, int arg, uint8_t attr,
			     int *ptr, uint32_t *saved)
{
	return atom_get_src_int(ctx, arg | atom_dst_to_src[(attr >> 3) & 7][(attr >> 6) & 3] << 3,
				ptr, saved);
}

static void atom_skip_dst(atom_exec_context *ctx, int arg, uint8_t attr, int *ptr)
{
	atom_skip_src_int(ctx, arg | atom_dst_to_src[(attr >> 3) & 7][(attr >> 6) & 3] << 3, ptr);
}

static void atom_put_dst(atom_exec_context *ctx, int arg, uint8_t attr,
			 int *ptr, uint32_t val, uint32_t saved)
{
	uint32_t align = atom_dst_to_src[(attr >> 3) & 7][(attr >> 6) & 3], idx;
	struct atom_context *gctx = ctx->ctx;
	val <<= atom_arg_shift[align];
	val &= atom_arg_mask[align];
	saved &= ~atom_arg_mask[align];
	val |= saved;
	switch (arg) {
	case ATOM_ARG_REG:
		idx = U16(*ptr);
		(*ptr) += 2;
		idx += gctx->reg_block;
		switch (gctx->io_mode) {
		case ATOM_IO_MM:
			// the xfx rv730 never completes pll_ctlreq_chg in setengineclock (spll) or memorypllinit (mpll)
			if (gctx->card->quirk_pll_ctlreq && (val & (1u << 23)) &&
			    ((ctx->start == U16(gctx->cmd_table + 4 + 2 * 0x0a) && idx == 0x181) ||
			     (ctx->start == U16(gctx->cmd_table + 4 + 2 * 0x10) && idx == 0x18a))) {
				if (!gctx->quirk_hits++)
					kprintf("atom: rv730 skipping wedged pll_ctlreq_chg writes\n");
				break;
			}
			TRACE("atom: %*sreg[%04x] <- %08x\n", atom_depth, "", idx, idx == 0 ? val << 2 : val);
			if (idx == 0)
				gctx->card->reg_write(gctx->card, idx, val << 2);
			else
				gctx->card->reg_write(gctx->card, idx, val);
			break;
		case ATOM_IO_PCI:
			kprintf("atom: pci registers are not implemented\n");
			return;
		case ATOM_IO_SYSIO:
			kprintf("atom: sysio registers are not implemented\n");
			return;
		default:
			if (!(gctx->io_mode & 0x80)) {
				kprintf("atom: bad io mode\n");
				return;
			}
			if (!gctx->iio[gctx->io_mode & 0xFF]) {
				kprintf("atom: undefined indirect io write method %d\n", gctx->io_mode & 0x7F);
				return;
			}
			atom_iio_execute(gctx, gctx->iio[gctx->io_mode & 0xFF], idx, val);
		}
		break;
	case ATOM_ARG_PS:
		idx = U8(*ptr);
		(*ptr)++;
		if ((int)idx >= ctx->ps_size) {
			kprintf("atom: ps index out of range: %d > %d\n", idx, ctx->ps_size);
			return;
		}
		ctx->ps[idx] = val;
		break;
	case ATOM_ARG_WS:
		idx = U8(*ptr);
		(*ptr)++;
		switch (idx) {
		case ATOM_WS_QUOTIENT:
			gctx->divmul[0] = val;
			break;
		case ATOM_WS_REMAINDER:
			gctx->divmul[1] = val;
			break;
		case ATOM_WS_DATAPTR:
			gctx->data_block = val;
			break;
		case ATOM_WS_SHIFT:
			gctx->shift = val;
			break;
		case ATOM_WS_OR_MASK:
		case ATOM_WS_AND_MASK:
			break;
		case ATOM_WS_FB_WINDOW:
			gctx->fb_base = val;
			break;
		case ATOM_WS_ATTRIBUTES:
			gctx->io_attr = val;
			break;
		case ATOM_WS_REGPTR:
			gctx->reg_block = val;
			break;
		default:
			if ((int)idx >= ctx->ws_size) {
				kprintf("atom: ws index out of range: %d > %d\n", idx, ctx->ws_size);
				return;
			}
			ctx->ws[idx] = val;
		}
		break;
	case ATOM_ARG_FB:
		idx = U8(*ptr);
		(*ptr)++;
		if ((gctx->fb_base + (idx * 4) + 4) > (uint32_t)gctx->scratch_size_bytes)
			kprintf("atom: fb write beyond scratch region: %d vs. %d\n",
				gctx->fb_base + (idx * 4), gctx->scratch_size_bytes);
		else
			gctx->scratch[(gctx->fb_base / 4) + idx] = val;
		break;
	case ATOM_ARG_PLL:
		idx = U8(*ptr);
		(*ptr)++;
		gctx->card->pll_write(gctx->card, idx, val);
		break;
	case ATOM_ARG_MC:
		idx = U8(*ptr);
		(*ptr)++;
		gctx->card->mc_write(gctx->card, idx, val);
		return;
	}
}

static void atom_op_add(atom_exec_context *ctx, int *ptr, int arg)
{
	uint8_t attr = U8((*ptr)++);
	uint32_t dst, src, saved;
	int dptr = *ptr;
	dst = atom_get_dst(ctx, arg, attr, ptr, &saved);
	src = atom_get_src(ctx, attr, ptr);
	dst += src;
	atom_put_dst(ctx, arg, attr, &dptr, dst, saved);
}

static void atom_op_and(atom_exec_context *ctx, int *ptr, int arg)
{
	uint8_t attr = U8((*ptr)++);
	uint32_t dst, src, saved;
	int dptr = *ptr;
	dst = atom_get_dst(ctx, arg, attr, ptr, &saved);
	src = atom_get_src(ctx, attr, ptr);
	dst &= src;
	atom_put_dst(ctx, arg, attr, &dptr, dst, saved);
}

static void atom_op_beep(atom_exec_context *ctx, int *ptr, int arg)
{
	kprintf("atom: bios beeped\n");
}

static void atom_op_calltable(atom_exec_context *ctx, int *ptr, int arg)
{
	int idx = U8((*ptr)++);
	int r = 0;

	TRACE("atom: %*scall table %d\n", atom_depth, "", idx);
	if (U16(ctx->ctx->cmd_table + 4 + 2 * idx))
		r = atom_execute_table_locked(ctx->ctx, idx, ctx->ps + ctx->ps_shift, ctx->ps_size - ctx->ps_shift);
	if (r)
		ctx->abort = true;
}

static void atom_op_clear(atom_exec_context *ctx, int *ptr, int arg)
{
	uint8_t attr = U8((*ptr)++);
	uint32_t saved;
	int dptr = *ptr;
	attr &= 0x38;
	attr |= atom_def_dst[attr >> 3] << 6;
	atom_get_dst(ctx, arg, attr, ptr, &saved);
	atom_put_dst(ctx, arg, attr, &dptr, 0, saved);
}

static void atom_op_compare(atom_exec_context *ctx, int *ptr, int arg)
{
	uint8_t attr = U8((*ptr)++);
	uint32_t dst, src;
	dst = atom_get_dst(ctx, arg, attr, ptr, NULL);
	src = atom_get_src(ctx, attr, ptr);
	ctx->ctx->cs_equal = (dst == src);
	ctx->ctx->cs_above = (dst > src);
}

static void atom_op_delay(atom_exec_context *ctx, int *ptr, int arg)
{
	unsigned count = U8((*ptr)++);
	if (arg == ATOM_UNIT_MICROSEC)
		IODelay(count);
	else
		IOSleep(count);
}

static void atom_op_div(atom_exec_context *ctx, int *ptr, int arg)
{
	uint8_t attr = U8((*ptr)++);
	uint32_t dst, src;
	dst = atom_get_dst(ctx, arg, attr, ptr, NULL);
	src = atom_get_src(ctx, attr, ptr);
	if (src != 0) {
		ctx->ctx->divmul[0] = dst / src;
		ctx->ctx->divmul[1] = dst % src;
	} else {
		ctx->ctx->divmul[0] = 0;
		ctx->ctx->divmul[1] = 0;
	}
}

static void atom_op_eot(atom_exec_context *ctx, int *ptr, int arg)
{
}

static void atom_op_jump(atom_exec_context *ctx, int *ptr, int arg)
{
	int execute = 0, target = U16(*ptr);

	(*ptr) += 2;
	switch (arg) {
	case ATOM_COND_ABOVE:
		execute = ctx->ctx->cs_above;
		break;
	case ATOM_COND_ABOVEOREQUAL:
		execute = ctx->ctx->cs_above || ctx->ctx->cs_equal;
		break;
	case ATOM_COND_ALWAYS:
		execute = 1;
		break;
	case ATOM_COND_BELOW:
		execute = !(ctx->ctx->cs_above || ctx->ctx->cs_equal);
		break;
	case ATOM_COND_BELOWOREQUAL:
		execute = !ctx->ctx->cs_above;
		break;
	case ATOM_COND_EQUAL:
		execute = ctx->ctx->cs_equal;
		break;
	case ATOM_COND_NOTEQUAL:
		execute = !ctx->ctx->cs_equal;
		break;
	}
	if (execute) {
		if (ctx->last_jump == (unsigned)(ctx->start + target)) {
			if (now_ms() - ctx->last_jump_ms > 5000) {
				kprintf("atom: stuck in a loop for more than 5 secs, aborting\n");
				ctx->abort = true;
			}
		} else {
			ctx->last_jump = ctx->start + target;
			ctx->last_jump_ms = now_ms();
		}
		*ptr = ctx->start + target;
	}
}

static void atom_op_mask(atom_exec_context *ctx, int *ptr, int arg)
{
	uint8_t attr = U8((*ptr)++);
	uint32_t dst, mask, src, saved;
	int dptr = *ptr;
	dst = atom_get_dst(ctx, arg, attr, ptr, &saved);
	mask = atom_get_src_direct(ctx, ((attr >> 3) & 7), ptr);
	src = atom_get_src(ctx, attr, ptr);
	dst &= mask;
	dst |= src;
	atom_put_dst(ctx, arg, attr, &dptr, dst, saved);
}

static void atom_op_move(atom_exec_context *ctx, int *ptr, int arg)
{
	uint8_t attr = U8((*ptr)++);
	uint32_t src, saved;
	int dptr = *ptr;
	if (((attr >> 3) & 7) != ATOM_SRC_DWORD)
		atom_get_dst(ctx, arg, attr, ptr, &saved);
	else {
		atom_skip_dst(ctx, arg, attr, ptr);
		saved = 0xCDCDCDCD;
	}
	src = atom_get_src(ctx, attr, ptr);
	atom_put_dst(ctx, arg, attr, &dptr, src, saved);
}

static void atom_op_mul(atom_exec_context *ctx, int *ptr, int arg)
{
	uint8_t attr = U8((*ptr)++);
	uint32_t dst, src;
	dst = atom_get_dst(ctx, arg, attr, ptr, NULL);
	src = atom_get_src(ctx, attr, ptr);
	ctx->ctx->divmul[0] = dst * src;
}

static void atom_op_nop(atom_exec_context *ctx, int *ptr, int arg)
{
}

static void atom_op_or(atom_exec_context *ctx, int *ptr, int arg)
{
	uint8_t attr = U8((*ptr)++);
	uint32_t dst, src, saved;
	int dptr = *ptr;
	dst = atom_get_dst(ctx, arg, attr, ptr, &saved);
	src = atom_get_src(ctx, attr, ptr);
	dst |= src;
	atom_put_dst(ctx, arg, attr, &dptr, dst, saved);
}

static void atom_op_postcard(atom_exec_context *ctx, int *ptr, int arg)
{
	(*ptr)++;
}

static void atom_op_unimplemented(atom_exec_context *ctx, int *ptr, int arg)
{
	kprintf("atom: unimplemented opcode\n");
}

static void atom_op_setdatablock(atom_exec_context *ctx, int *ptr, int arg)
{
	int idx = U8(*ptr);
	(*ptr)++;
	if (!idx)
		ctx->ctx->data_block = 0;
	else if (idx == 255)
		ctx->ctx->data_block = ctx->start;
	else
		ctx->ctx->data_block = U16(ctx->ctx->data_table + 4 + 2 * idx);
}

static void atom_op_setfbbase(atom_exec_context *ctx, int *ptr, int arg)
{
	uint8_t attr = U8((*ptr)++);
	ctx->ctx->fb_base = atom_get_src(ctx, attr, ptr);
}

static void atom_op_setport(atom_exec_context *ctx, int *ptr, int arg)
{
	int port;
	switch (arg) {
	case ATOM_PORT_ATI:
		port = U16(*ptr);
		if (!port)
			ctx->ctx->io_mode = ATOM_IO_MM;
		else
			ctx->ctx->io_mode = ATOM_IO_IIO | port;
		(*ptr) += 2;
		break;
	case ATOM_PORT_PCI:
		ctx->ctx->io_mode = ATOM_IO_PCI;
		(*ptr)++;
		break;
	case ATOM_PORT_SYSIO:
		ctx->ctx->io_mode = ATOM_IO_SYSIO;
		(*ptr)++;
		break;
	}
}

static void atom_op_setregblock(atom_exec_context *ctx, int *ptr, int arg)
{
	ctx->ctx->reg_block = U16(*ptr);
	(*ptr) += 2;
}

static void atom_op_shift_left(atom_exec_context *ctx, int *ptr, int arg)
{
	uint8_t attr = U8((*ptr)++), shift;
	uint32_t saved, dst;
	int dptr = *ptr;
	attr &= 0x38;
	attr |= atom_def_dst[attr >> 3] << 6;
	dst = atom_get_dst(ctx, arg, attr, ptr, &saved);
	shift = atom_get_src_direct(ctx, ATOM_SRC_BYTE0, ptr);
	dst <<= shift;
	atom_put_dst(ctx, arg, attr, &dptr, dst, saved);
}

static void atom_op_shift_right(atom_exec_context *ctx, int *ptr, int arg)
{
	uint8_t attr = U8((*ptr)++), shift;
	uint32_t saved, dst;
	int dptr = *ptr;
	attr &= 0x38;
	attr |= atom_def_dst[attr >> 3] << 6;
	dst = atom_get_dst(ctx, arg, attr, ptr, &saved);
	shift = atom_get_src_direct(ctx, ATOM_SRC_BYTE0, ptr);
	dst >>= shift;
	atom_put_dst(ctx, arg, attr, &dptr, dst, saved);
}

static void atom_op_shl(atom_exec_context *ctx, int *ptr, int arg)
{
	uint8_t attr = U8((*ptr)++), shift;
	uint32_t saved, dst;
	int dptr = *ptr;
	uint32_t dst_align = atom_dst_to_src[(attr >> 3) & 7][(attr >> 6) & 3];
	dst = atom_get_dst(ctx, arg, attr, ptr, &saved);
	// op needs the full dst value
	dst = saved;
	shift = atom_get_src(ctx, attr, ptr);
	dst <<= shift;
	dst &= atom_arg_mask[dst_align];
	dst >>= atom_arg_shift[dst_align];
	atom_put_dst(ctx, arg, attr, &dptr, dst, saved);
}

static void atom_op_shr(atom_exec_context *ctx, int *ptr, int arg)
{
	uint8_t attr = U8((*ptr)++), shift;
	uint32_t saved, dst;
	int dptr = *ptr;
	uint32_t dst_align = atom_dst_to_src[(attr >> 3) & 7][(attr >> 6) & 3];
	dst = atom_get_dst(ctx, arg, attr, ptr, &saved);
	// op needs the full dst value
	dst = saved;
	shift = atom_get_src(ctx, attr, ptr);
	dst >>= shift;
	dst &= atom_arg_mask[dst_align];
	dst >>= atom_arg_shift[dst_align];
	atom_put_dst(ctx, arg, attr, &dptr, dst, saved);
}

static void atom_op_sub(atom_exec_context *ctx, int *ptr, int arg)
{
	uint8_t attr = U8((*ptr)++);
	uint32_t dst, src, saved;
	int dptr = *ptr;
	dst = atom_get_dst(ctx, arg, attr, ptr, &saved);
	src = atom_get_src(ctx, attr, ptr);
	dst -= src;
	atom_put_dst(ctx, arg, attr, &dptr, dst, saved);
}

static void atom_op_switch(atom_exec_context *ctx, int *ptr, int arg)
{
	uint8_t attr = U8((*ptr)++);
	uint32_t src, val, target;
	src = atom_get_src(ctx, attr, ptr);
	while (U16(*ptr) != ATOM_CASE_END)
		if (U8(*ptr) == ATOM_CASE_MAGIC) {
			(*ptr)++;
			val = atom_get_src(ctx, (attr & 0x38) | ATOM_ARG_IMM, ptr);
			target = U16(*ptr);
			if (val == src) {
				*ptr = ctx->start + target;
				return;
			}
			(*ptr) += 2;
		} else {
			kprintf("atom: bad case\n");
			return;
		}
	(*ptr) += 2;
}

static void atom_op_test(atom_exec_context *ctx, int *ptr, int arg)
{
	uint8_t attr = U8((*ptr)++);
	uint32_t dst, src;
	dst = atom_get_dst(ctx, arg, attr, ptr, NULL);
	src = atom_get_src(ctx, attr, ptr);
	ctx->ctx->cs_equal = ((dst & src) == 0);
}

static void atom_op_xor(atom_exec_context *ctx, int *ptr, int arg)
{
	uint8_t attr = U8((*ptr)++);
	uint32_t dst, src, saved;
	int dptr = *ptr;
	dst = atom_get_dst(ctx, arg, attr, ptr, &saved);
	src = atom_get_src(ctx, attr, ptr);
	dst ^= src;
	atom_put_dst(ctx, arg, attr, &dptr, dst, saved);
}

#define ARG6(fn) \
	{ fn, ATOM_ARG_REG }, { fn, ATOM_ARG_PS }, { fn, ATOM_ARG_WS }, \
	{ fn, ATOM_ARG_FB }, { fn, ATOM_ARG_PLL }, { fn, ATOM_ARG_MC }

static struct {
	void (*func)(atom_exec_context *, int *, int);
	int arg;
} opcode_table[ATOM_OP_CNT] = {
	{ NULL, 0 },
	ARG6(atom_op_move),
	ARG6(atom_op_and),
	ARG6(atom_op_or),
	ARG6(atom_op_shift_left),
	ARG6(atom_op_shift_right),
	ARG6(atom_op_mul),
	ARG6(atom_op_div),
	ARG6(atom_op_add),
	ARG6(atom_op_sub),
	{ atom_op_setport, ATOM_PORT_ATI }, { atom_op_setport, ATOM_PORT_PCI },
	{ atom_op_setport, ATOM_PORT_SYSIO }, { atom_op_setregblock, 0 },
	{ atom_op_setfbbase, 0 },
	ARG6(atom_op_compare),
	{ atom_op_switch, 0 },
	{ atom_op_jump, ATOM_COND_ALWAYS }, { atom_op_jump, ATOM_COND_EQUAL },
	{ atom_op_jump, ATOM_COND_BELOW }, { atom_op_jump, ATOM_COND_ABOVE },
	{ atom_op_jump, ATOM_COND_BELOWOREQUAL }, { atom_op_jump, ATOM_COND_ABOVEOREQUAL },
	{ atom_op_jump, ATOM_COND_NOTEQUAL },
	ARG6(atom_op_test),
	{ atom_op_delay, ATOM_UNIT_MILLISEC }, { atom_op_delay, ATOM_UNIT_MICROSEC },
	{ atom_op_calltable, 0 }, { atom_op_unimplemented, 0 },
	ARG6(atom_op_clear),
	{ atom_op_nop, 0 }, { atom_op_eot, 0 },
	ARG6(atom_op_mask),
	{ atom_op_postcard, 0 }, { atom_op_beep, 0 },
	{ atom_op_unimplemented, 0 }, { atom_op_unimplemented, 0 },
	{ atom_op_setdatablock, 0 },
	ARG6(atom_op_xor),
	ARG6(atom_op_shl),
	ARG6(atom_op_shr),
	{ atom_op_unimplemented, 0 },
};

static int atom_execute_table_locked(struct atom_context *ctx, int index, uint32_t *params, int params_size)
{
	int base = CU16(ctx->cmd_table + 4 + 2 * index);
	int len, ws, ps, ptr;
	unsigned char op;
	atom_exec_context ectx;
	int ret = 0;

	if (!base)
		return -1;

	len = CU16(base + ATOM_CT_SIZE_PTR);
	ws = CU8(base + ATOM_CT_WS_PTR);
	ps = CU8(base + ATOM_CT_PS_PTR) & ATOM_CT_PS_MASK;
	ptr = base + ATOM_CT_CODE_PTR;

	TRACE("atom: %*s>> execute table %d @%04x (len %d, ws %d, ps %d)\n", atom_depth, "", index, base, len, ws, ps);

	ectx.ctx = ctx;
	ectx.ps_shift = ps / 4;
	ectx.start = base;
	ectx.ps = params;
	ectx.ps_size = params_size;
	ectx.abort = false;
	ectx.last_jump = 0;
	ectx.last_jump_ms = 0;
	if (ws) {
		ectx.ws = (uint32_t *)IOMallocZero(4 * ws);
		ectx.ws_size = ws;
	} else {
		ectx.ws = NULL;
		ectx.ws_size = 0;
	}

	atom_depth++;
	while (1) {
		op = CU8(ptr++);
		if (ectx.abort) {
			kprintf("atom: stuck executing %04x (len %d, ws %d, ps %d) @ 0x%04x\n", base, len, ws, ps, ptr - 1);
			ret = -1;
			break;
		}

		if (op < ATOM_OP_CNT && op > 0)
			opcode_table[op].func(&ectx, &ptr, opcode_table[op].arg);
		else
			break;

		if (op == ATOM_OP_EOT)
			break;
	}
	atom_depth--;

	if (ectx.ws)
		IOFree(ectx.ws, 4 * ws);
	return ret;
}

int atom_execute_table(struct atom_context *ctx, int index, uint32_t *params, int params_size)
{
	int r;
	uint8_t *tail = ctx->scratch ? (uint8_t *)ctx->scratch + ctx->scratch_size_bytes : NULL;

	// reset data block, reg block, fb window, io mode and divmul
	ctx->data_block = 0;
	ctx->reg_block = 0;
	ctx->fb_base = 0;
	ctx->io_mode = ATOM_IO_MM;
	ctx->divmul[0] = 0;
	ctx->divmul[1] = 0;

	if (tail && ctx->scratch_slack)
		memset(tail, 0xa5, ctx->scratch_slack);

	r = atom_execute_table_locked(ctx, index, params, params_size);

	if (tail && ctx->scratch_slack) {
		for (int i = 0; i < ctx->scratch_slack; i++)
			if (tail[i] != 0xa5) {
				kprintf("atom: table %d wrote %d bytes past the scratch end (byte %02x)\n", index, i, tail[i]);
				break;
			}
	}
	return r;
}

static int atom_iio_len[] = { 1, 2, 3, 3, 3, 3, 4, 4, 4, 3 };

static void atom_index_iio(struct atom_context *ctx, int base)
{
	ctx->iio = (uint16_t *)IOMallocZero(2 * 256);
	if (!ctx->iio)
		return;
	while (CU8(base) == ATOM_IIO_START) {
		ctx->iio[CU8(base + 1)] = base + 2;
		base += 2;
		while (CU8(base) != ATOM_IIO_END)
			base += atom_iio_len[CU8(base)];
		base += 3;
	}
}

struct atom_context *atom_parse(struct atom_card *card, void *bios, uint32_t size)
{
	int base;
	struct atom_context *ctx = (struct atom_context *)IOMallocZero(sizeof(*ctx));
	int i;

	if (!ctx)
		return NULL;

	ctx->card = card;
	ctx->bios = bios;
	ctx->bios_size = size;

	if (CU16(0) != ATOM_BIOS_MAGIC) {
		kprintf("atom: invalid bios magic\n");
		IOFree(ctx, sizeof(*ctx));
		return NULL;
	}
	if (memcmp((char *)bios + ATOM_ATI_MAGIC_PTR, ATOM_ATI_MAGIC, strlen(ATOM_ATI_MAGIC))) {
		kprintf("atom: invalid ati magic\n");
		IOFree(ctx, sizeof(*ctx));
		return NULL;
	}

	base = CU16(ATOM_ROM_TABLE_PTR);
	if (memcmp((char *)bios + base + ATOM_ROM_MAGIC_PTR, ATOM_ROM_MAGIC, strlen(ATOM_ROM_MAGIC))) {
		kprintf("atom: invalid atom magic\n");
		IOFree(ctx, sizeof(*ctx));
		return NULL;
	}

	ctx->cmd_table = CU16(base + ATOM_ROM_CMD_PTR);
	ctx->data_table = CU16(base + ATOM_ROM_DATA_PTR);
	atom_index_iio(ctx, CU16(ctx->data_table + ATOM_DATA_IIO_PTR) + 4);
	if (!ctx->iio) {
		atom_destroy(ctx);
		return NULL;
	}

	// the name string isn't always terminated
	int sp = CU16(base + ATOM_ROM_MSG_PTR);
	while (CU8(sp) == '\n' || CU8(sp) == '\r')
		sp++;
	for (i = 0; i < (int)sizeof(ctx->name) - 1; i++) {
		char ch = CU8(sp + i);
		if (ch < '.' || ch > 'z')
			break;
		ctx->name[i] = ch;
	}
	ctx->name[i] = 0;

	return ctx;
}

int atom_asic_init(struct atom_context *ctx)
{
	int hwi = CU16(ctx->data_table + ATOM_DATA_FWI_PTR);
	uint32_t ps[16];
	int ret;

	memset(ps, 0, 64);

	ps[0] = CU32(hwi + ATOM_FWI_DEFSCLK_PTR);
	ps[1] = CU32(hwi + ATOM_FWI_DEFMCLK_PTR);
	if (!ps[0] || !ps[1])
		return 1;

	if (!CU16(ctx->cmd_table + 4 + 2 * ATOM_CMD_INIT))
		return 1;
	ret = atom_execute_table(ctx, ATOM_CMD_INIT, ps, 16);
	return ret;
}

void atom_destroy(struct atom_context *ctx)
{
	if (ctx->iio)
		IOFree(ctx->iio, 2 * 256);
	if (ctx->scratch)
		IOFree(ctx->scratch, ctx->scratch_size_bytes + ctx->scratch_slack);
	IOFree(ctx, sizeof(*ctx));
}

bool atom_parse_data_header(struct atom_context *ctx, int index,
			    uint16_t *size, uint8_t *frev, uint8_t *crev,
			    uint16_t *data_start)
{
	int offset = index * 2 + 4;
	int idx = CU16(ctx->data_table + offset);

	if (!idx)
		return false;

	if (size)
		*size = CU16(idx);
	if (frev)
		*frev = CU8(idx + 2);
	if (crev)
		*crev = CU8(idx + 3);
	*data_start = idx;
	return true;
}

bool atom_parse_cmd_header(struct atom_context *ctx, int index, uint8_t *frev,
			   uint8_t *crev)
{
	int offset = index * 2 + 4;
	int idx = CU16(ctx->cmd_table + offset);

	if (!idx)
		return false;

	if (frev)
		*frev = CU8(idx + 2);
	if (crev)
		*crev = CU8(idx + 3);
	return true;
}

int atom_allocate_fb_scratch(struct atom_context *ctx, int slack)
{
	int index = GetIndexIntoMasterTable(DATA, VRAM_UsageByFirmware);
	uint16_t data_offset;
	int usage_bytes = 0;

	if (atom_parse_data_header(ctx, index, NULL, NULL, NULL, &data_offset))
		usage_bytes = CU16(data_offset + 8) * 1024;
	ctx->scratch_size_bytes = 0;
	if (usage_bytes == 0)
		usage_bytes = 20 * 1024;
	ctx->scratch_slack = slack;
	ctx->scratch = (uint32_t *)IOMallocZero(usage_bytes + slack);
	if (!ctx->scratch)
		return -1;
	ctx->scratch_size_bytes = usage_bytes;
	return 0;
}

/*
 * WKdm codec stand-in for ARM1176 (ARMv6).
 *
 * The tree's WKdm implementations are per-architecture assembly, and the arm
 * one is written for NEON and ARMv6T2 - neither of which this core has. Rather
 * than hand-write an ARMv6 codec during bring-up, report every page as
 * incompressible: the compressor then stores pages verbatim and never has a
 * WKdm-compressed page to hand back, so the decompressor is unreachable.
 *
 * The cost is memory, not correctness. Replace with a real implementation (a
 * portable C WKdm, or an ARMv6 rewrite of the assembly) before this board is
 * expected to run under memory pressure.
 */
#include <pexpert/arm/board_config.h>

#if defined (ARM1176)

#include <vm/WKdm_new.h>

int
WKdm_compress_new(const WK_word *src_buf, WK_word *dest_buf, WK_word *scratch,
    unsigned int limit)
{
	(void)src_buf;
	(void)dest_buf;
	(void)scratch;
	(void)limit;
	return -1;      /* incompressible */
}

void
WKdm_decompress_new(WK_word *src_buf, WK_word *dest_buf, WK_word *scratch,
    unsigned int bytes)
{
	(void)src_buf;
	(void)dest_buf;
	(void)scratch;
	(void)bytes;
	panic("WKdm_decompress_new: no WKdm codec on this board");
}

#endif /* ARM1176 */

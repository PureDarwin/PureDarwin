/*-
 * Copyright (c) 1998 Matthew Dillon.  All Rights Reserved.
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 * GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * BLIST.C -	Bitmap allocator/deallocator, using a radix tree with hinting
 *
 *	This module implements a general bitmap allocator/deallocator.  The
 *	allocator eats around 2 bits per 'block'.  The module does not
 *	try to interpret the meaning of a 'block' other then to return
 *	SWAPBLK_NONE on an allocation failure.
 *
 *	A radix tree is used to maintain the bitmap.  Two radix constants are
 *	involved:  One for the bitmaps contained in the leaf nodes (typically
 *	32), and one for the meta nodes (typically 16).  Both meta and leaf
 *	nodes have a hint field.  This field gives us a hint as to the largest
 *	free contiguous range of blocks under the node.  It may contain a
 *	value that is too high, but will never contain a value that is too
 *	low.  When the radix tree is searched, allocation failures in subtrees
 *	update the hint.
 *
 *	The radix tree also implements two collapsed states for meta nodes:
 *	the ALL-ALLOCATED state and the ALL-FREE state.  If a meta node is
 *	in either of these two states, all information contained underneath
 *	the node is considered stale.  These states are used to optimize
 *	allocation and freeing operations.
 *
 * 	The hinting greatly increases code efficiency for allocations while
 *	the general radix structure optimizes both allocations and frees.  The
 *	radix tree should be able to operate well no matter how much
 *	fragmentation there is and no matter how large a bitmap is used.
 *
 *	Unlike the rlist code, the blist code wires all necessary memory at
 *	creation time.  Neither allocations nor frees require interaction with
 *	the memory subsystem.  In contrast, the rlist code may allocate memory
 *	on an rlist_free() call.  The non-blocking features of the blist code
 *	are used to great advantage in the swap code (vm/nswap_pager.c).  The
 *	rlist code uses a little less overall memory then the blist code (but
 *	due to swap interleaving not all that much less), but the blist code
 *	scales much, much better.
 *
 *	LAYOUT: The radix tree is layed out recursively using a
 *	linear array.  Each meta node is immediately followed (layed out
 *	sequentially in memory) by BLIST_META_RADIX lower level nodes.  This
 *	is a recursive structure but one that can be easily scanned through
 *	a very simple 'skip' calculation.  In order to support large radixes,
 *	portions of the tree may reside outside our memory allocation.  We
 *	handle this with an early-termination optimization (when bighint is
 *	set to -1) on the scan.  The memory allocation is only large enough
 *	to cover the number of blocks requested at creation time even if it
 *	must be encompassed in larger root-node radix.
 *
 *	NOTE: the allocator cannot currently allocate more then
 *	BLIST_BMAP_RADIX blocks per call.  It will panic with 'allocation too
 *	large' if you try.  This is an area that could use improvement.  The
 *	radix is large enough that this restriction does not effect the swap
 *	system, though.  Currently only the allocation code is effected by
 *	this algorithmic unfeature.  The freeing code can handle arbitrary
 *	ranges.
 *
 *	This code can be compiled stand-alone for debugging.
 */

/*
 *  NOTE: a few modifications is made in the orginal FreeBSD code:
 *  1. change the name of some variables/constants
 *  2. discard the code handling ALL_FREE and ALL_ALLOCATED state
 *  3. allocate more than 32 slots won't lead to kernel panic
 *  4. remove all the code for debugging
 *
 *                                                      Zhao Shuai
 *                                                   upczhsh@163.com
 */


#include <stdlib.h>
#include <util/RadixBitmap.h>

#define TERMINATOR -1


static uint32
radix_bitmap_init(radix_node *node, uint32 radix, uint32 skip, uint32 slots)
{
	uint32 index = 0;
	int32 big_hint = radix < slots ? radix : slots;

	// leaf node
	if (radix == BITMAP_RADIX) {
		if (node) {
			node->big_hint = big_hint;
			if (big_hint == BITMAP_RADIX)
				node->u.bitmap = 0;
			else
				node->u.bitmap = (bitmap_t)-1 << big_hint;
		}
		return index;
	}

	// not leaf node
	if (node) {
		node->big_hint = big_hint;
		node->u.available = big_hint;
	}

	radix /= NODE_RADIX;
	uint32 next_skip = skip / NODE_RADIX;

	uint32 i;
	for (i = 1; i <= skip; i += next_skip) {
		if (slots >= radix) {
			index = i + radix_bitmap_init(node ? &node[i] : NULL,
					radix, next_skip - 1, radix);
			slots -= radix;
		} else if (slots > 0) {
			index = i + radix_bitmap_init(node ? &node[i] : NULL,
					radix, next_skip - 1, slots);
			slots = 0;
		} else { // add a terminator
			if (node)
				node[i].big_hint = TERMINATOR;
			break;
		}
	}

	if (index < i)
		index = i;

	return index;
}


radix_bitmap *
radix_bitmap_create(uint32 slots)
{
	uint32 radix = BITMAP_RADIX;
	uint32 skip = 0;

	while (radix < slots) {
		radix *= NODE_RADIX;
		skip = (skip + 1) * NODE_RADIX;
	}

	radix_bitmap *bmp = (radix_bitmap *)malloc(sizeof(radix_bitmap));
	if (bmp == NULL)
		return NULL;

	bmp->radix = radix;
	bmp->skip = skip;
	bmp->free_slots = slots;
	bmp->root_size = 1 + radix_bitmap_init(NULL, radix, skip, slots);

	bmp->root = (radix_node *)malloc(bmp->root_size * sizeof(radix_node));
	if (bmp->root == NULL) {
		free(bmp);
		return NULL;
	}

	radix_bitmap_init(bmp->root, radix, skip, slots);

	return bmp;
}


void
radix_bitmap_destroy(radix_bitmap *bmp)
{
	free(bmp->root);
	free(bmp);
}


static radix_slot_t
radix_leaf_alloc(radix_node *leaf, radix_slot_t slotIndex, int32 count)
{
	if (count <= (int32)BITMAP_RADIX) {
		bitmap_t bitmap = ~leaf->u.bitmap;
		uint32 n = BITMAP_RADIX - count;
		bitmap_t mask = (bitmap_t)-1 >> n;
		for (uint32 j = 0; j <= n; j++) {
			if ((bitmap & mask) == mask) {
				leaf->u.bitmap |= mask;
				return (slotIndex + j);
			}
			mask <<= 1;
		}
	}

	// we could not allocate count here, update big_hint
	if (leaf->big_hint >= count)
		leaf->big_hint = count - 1;
	return RADIX_SLOT_NONE;
}


static radix_slot_t
radix_node_alloc(radix_node *node, radix_slot_t slotIndex, int32 count,
		uint32 radix, uint32 skip)
{
	uint32 next_skip = skip / NODE_RADIX;
	radix /= NODE_RADIX;

	for (uint32 i = 1; i <= skip; i += next_skip) {
		if (node[i].big_hint == TERMINATOR)  // TERMINATOR
			break;

		if (count <= node[i].big_hint) {
			radix_slot_t addr = RADIX_SLOT_NONE;
			if (next_skip == 1)
				addr = radix_leaf_alloc(&node[i], slotIndex, count);
			else
				addr = radix_node_alloc(&node[i], slotIndex, count, radix,
						next_skip - 1);
			if (addr != RADIX_SLOT_NONE) {
				node->u.available -= count;
				if (node->big_hint > node->u.available)
					node->big_hint = node->u.available;

				return addr;
			}
		}
		slotIndex += radix;
	}

	// we could not allocate count in the subtree, update big_hint
	if (node->big_hint >= count)
		node->big_hint = count - 1;
	return RADIX_SLOT_NONE;
}


radix_slot_t
radix_bitmap_alloc(radix_bitmap *bmp, uint32 count)
{
	radix_slot_t addr = RADIX_SLOT_NONE;

	if (bmp->radix == BITMAP_RADIX)
		addr = radix_leaf_alloc(bmp->root, 0, count);
	else
		addr = radix_node_alloc(bmp->root, 0, count, bmp->radix, bmp->skip);

	if (addr != RADIX_SLOT_NONE)
		bmp->free_slots -= count;

	return addr;
}


static void
radix_leaf_dealloc(radix_node *leaf, radix_slot_t slotIndex, uint32 count)
{
	uint32 n = slotIndex & (BITMAP_RADIX - 1);
	bitmap_t mask = ((bitmap_t)-1 >> (BITMAP_RADIX - count - n))
		& ((bitmap_t)-1 << n);
	leaf->u.bitmap &= ~mask;

	leaf->big_hint = BITMAP_RADIX;
}


static void
radix_node_dealloc(radix_node *node, radix_slot_t slotIndex, uint32 count,
		uint32 radix, uint32 skip, radix_slot_t index)
{
	node->u.available += count;

	uint32 next_skip = skip / NODE_RADIX;
	radix /= NODE_RADIX;

	uint32 i = (slotIndex - index) / radix;
	index += i * radix;
	i = i * next_skip + 1;

	while (i <= skip && index < slotIndex + count) {
		uint32 v = index + radix - slotIndex;
		if (v > count)
			v = count;

		if (next_skip == 1)
			radix_leaf_dealloc(&node[i], slotIndex, v);
		else
			radix_node_dealloc(&node[i], slotIndex, v, radix,
					next_skip - 1, index);

		if (node->big_hint < node[i].big_hint)
			node->big_hint = node[i].big_hint;

		count -= v;
		slotIndex += v;
		index += radix;
		i += next_skip;
	}
}


void
radix_bitmap_dealloc(radix_bitmap *bmp, radix_slot_t slotIndex, uint32 count)
{
	if (bmp->radix == BITMAP_RADIX)
		radix_leaf_dealloc(bmp->root, slotIndex, count);
	else
		radix_node_dealloc(bmp->root, slotIndex, count, bmp->radix,
				bmp->skip, 0);

	bmp->free_slots += count;
}


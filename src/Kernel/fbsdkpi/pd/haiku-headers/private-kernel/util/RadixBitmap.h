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
 * The radix bitmap structure is ported from FreeBSD.
 */


#ifndef _KERNEL_UTIL_RADIX_BITMAP_H
#define _KERNEL_UTIL_RADIX_BITMAP_H


#include <SupportDefs.h>


typedef uint32 radix_slot_t;
typedef uint32 bitmap_t;

typedef struct radix_node {
	union {
		bitmap_t  bitmap;      // bitmap for the slots if we are a leaf
		int32     available;   // available slots under us if we are not a leaf
	} u;
	int32  big_hint;  // the biggest continuous slots under us
} radix_node;

// Bitmap which uses radix tree for hinting.
// The radix tree is stored in an array.
typedef struct radix_bitmap {
	radix_slot_t	free_slots;	// # of free slots
	radix_slot_t	radix;		// coverage radix
	radix_slot_t	skip;		// starting skip
	radix_node		*root;		// root of radix tree, actually it is an array
	radix_slot_t	root_size;	// size of the array(# of nodes in the tree)
} radix_bitmap;


#define BITMAP_RADIX  (sizeof(radix_slot_t) * 8)
#define NODE_RADIX    16

#define RADIX_SLOT_NONE	((radix_slot_t)-1)


extern radix_bitmap *radix_bitmap_create(uint32 slots);
extern radix_slot_t radix_bitmap_alloc(radix_bitmap *bmp, uint32 count);
extern void radix_bitmap_dealloc(radix_bitmap *bmp, radix_slot_t slotIndex,
		uint32 count);
extern void radix_bitmap_destroy(radix_bitmap *bmp);

#endif  // _KERNEL_UTIL_RADIX_BITMAP_H


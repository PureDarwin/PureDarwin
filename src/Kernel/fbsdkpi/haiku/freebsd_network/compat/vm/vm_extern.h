/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)vm_extern.h	8.2 (Berkeley) 1/12/94
 * $FreeBSD$
 */

#ifndef _VM_EXTERN_H_
#define	_VM_EXTERN_H_

/*
 * Is pa a multiple of alignment, which is a power-of-two?
 */
static inline bool
vm_addr_align_ok(vm_paddr_t pa, u_long alignment)
{
	KASSERT(powerof2(alignment), ("%s: alignment is not a power of 2: %#lx",
		__func__, alignment));
	return ((pa & (alignment - 1)) == 0);
}

/*
 * Do the first and last addresses of a range match in all bits except the ones
 * in -boundary (a power-of-two)?  For boundary == 0, all addresses match.
 */
static inline bool
vm_addr_bound_ok(vm_paddr_t pa, vm_paddr_t size, vm_paddr_t boundary)
{
	KASSERT(powerof2(boundary), ("%s: boundary is not a power of 2: %#jx",
		__func__, (uintmax_t)boundary));
	return (((pa ^ (pa + size - 1)) & -boundary) == 0);
}

static inline bool
vm_addr_ok(vm_paddr_t pa, vm_paddr_t size, u_long alignment,
	vm_paddr_t boundary)
{
	return (vm_addr_align_ok(pa, alignment) &&
		vm_addr_bound_ok(pa, size, boundary));
}

#endif				/* !_VM_EXTERN_H_ */

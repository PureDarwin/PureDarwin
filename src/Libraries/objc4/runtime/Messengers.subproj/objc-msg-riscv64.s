/*
 * @APPLE_LICENSE_HEADER_START@
 *
 * Copyright (c) 2011 Apple Inc.  All Rights Reserved.
 *
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 *
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 *
 * @APPLE_LICENSE_HEADER_END@
 */
// objc-msg-riscv64.s - riscv64 code to support objc messaging

#if defined(__riscv) && __riscv_xlen == 64

#include "isa.h"
#include "objc-config.h"

// register use, arm64's in brackets: t0 class [x16], t1 imp [x17], t2 bucket sel, stret flag on dispatch [x9],
// t3 bucket cursor, dead at calls [x13], t4 buckets [x10], t5 first probed [x12], t6 last [x11]. msgLookup returns t1 imp, t0 class, t2 stret.

#if CACHE_MASK_STORAGE != CACHE_MASK_STORAGE_HIGH_16_BIG_ADDRS
#error riscv64 msgSend expects the mask in the top 16 bits
#endif
#if CONFIG_USE_PREOPT_CACHES
#error riscv64 msgSend has no preoptimized cache support
#endif
#if CACHE_IMP_ENCODING != CACHE_IMP_ENCODING_ISA_XOR
#error riscv64 msgSend expects isa ^ imp in the cache
#endif
#if ISA_MASK != 0x0000003ffffffff8ULL
#error ExtractISA shifts do not match ISA_MASK
#endif

// ISA_MASK keeps bits 3..37
#define ISA_MASK_HI_SHIFT 26
#define ISA_MASK_LO_SHIFT 29

#define SUPPORT_TAGGED_POINTERS 1

.data

// _objc_restartableRanges lets method dispatch caching find threads active in the cache.
// The labels surround the cache lookup code, the tables are zero-terminated.

.macro RestartableEntry name
	.quad	LLookupStart\name
	.short	LLookupEnd\name - LLookupStart\name
	.short	LLookupRecover\name - LLookupStart\name
	.long	0
.endm

	.p2align 4
	.private_extern _objc_restartableRanges
_objc_restartableRanges:
	RestartableEntry _cache_getImp
	RestartableEntry _objc_msgSend
	RestartableEntry _objc_msgSendSuper2
	RestartableEntry _objc_msgLookup
	RestartableEntry _objc_msgLookupSuper2
	RestartableEntry _objc_msgSend_stret
	RestartableEntry _objc_msgSendSuper2_stret
	RestartableEntry _objc_msgLookup_stret
	RestartableEntry _objc_msgLookupSuper2_stret
	.fill	16, 1, 0


/* objc_super parameter to sendSuper */
#define RECEIVER         0
#define CLASS            8

/* Selected field offsets in class structure */
#define SUPERCLASS       8
#define CACHE            16

/* Selected field offsets in method structure */
#define METHOD_NAME      0
#define METHOD_TYPES     8
#define METHOD_IMP       16

/* bucket_t is {sel, imp} on riscv64 */
#define BUCKET_SEL       0
#define BUCKET_IMP       8
#define BUCKET_SIZE      16


// ExtractISA dst, src: src is a raw isa field, sets dst to the class pointer.

.macro ExtractISA dst, src
	slli	\dst, \src, ISA_MASK_HI_SHIFT
	srli	\dst, \dst, ISA_MASK_LO_SHIFT
	slli	\dst, \dst, 3
.endm


// ENTRY / STATIC_ENTRY / END_ENTRY functionName
// every entry gets a dwarf fde, riscv mach-o has no compact unwind

.macro ENTRY name
	.text
	.p2align 5
	.globl	\name
\name:
	.cfi_startproc
.endm

.macro STATIC_ENTRY name
	.text
	.p2align 5
	.private_extern \name
\name:
	.cfi_startproc
.endm

.macro END_ENTRY name
	.cfi_endproc
LExit\name:
.endm


#define NORMAL 0
#define GETIMP 1
#define LOOKUP 2
#define STRET  3
#define STRET_LOOKUP 4

#define MSGSEND 100
#define METHOD_INVOKE 101
#define MSGSEND_STRET 102
#define METHOD_INVOKE_STRET 103

// SAVE_REGS: create a stack frame and save all argument registers for a function call.
// Standard riscv frame record, s0 is the caller's sp with ra at s0-8 and the caller's s0 at s0-16.

#define FRAME_SIZE 160

.macro SAVE_REGS kind
	addi	sp, sp, -FRAME_SIZE
	.cfi_def_cfa_offset FRAME_SIZE
	sd	ra, FRAME_SIZE-8(sp)
	sd	s0, FRAME_SIZE-16(sp)
	.cfi_offset ra, -8
	.cfi_offset s0, -16
	addi	s0, sp, FRAME_SIZE
	.cfi_def_cfa s0, 0

	// save parameter registers: a0..a7, fa0..fa7
	sd	a0, 0*8(sp)
	sd	a1, 1*8(sp)
	sd	a2, 2*8(sp)
	sd	a3, 3*8(sp)
	sd	a4, 4*8(sp)
	sd	a5, 5*8(sp)
	sd	a6, 6*8(sp)
	sd	a7, 7*8(sp)
	fsd	fa0, 8*8(sp)
	fsd	fa1, 9*8(sp)
	fsd	fa2, 10*8(sp)
	fsd	fa3, 11*8(sp)
	fsd	fa4, 12*8(sp)
	fsd	fa5, 13*8(sp)
	fsd	fa6, 14*8(sp)
	fsd	fa7, 15*8(sp)
.if \kind == MSGSEND || \kind == MSGSEND_STRET
	sd	t0, 16*8(sp)
.elseif \kind != METHOD_INVOKE && \kind != METHOD_INVOKE_STRET
.abort Unknown kind.
.endif
.endm


// RESTORE_REGS: restore all argument registers and pop the stack frame created by SAVE_REGS.

.macro RESTORE_REGS kind
	ld	a0, 0*8(sp)
	ld	a1, 1*8(sp)
	ld	a2, 2*8(sp)
	ld	a3, 3*8(sp)
	ld	a4, 4*8(sp)
	ld	a5, 5*8(sp)
	ld	a6, 6*8(sp)
	ld	a7, 7*8(sp)
	fld	fa0, 8*8(sp)
	fld	fa1, 9*8(sp)
	fld	fa2, 10*8(sp)
	fld	fa3, 11*8(sp)
	fld	fa4, 12*8(sp)
	fld	fa5, 13*8(sp)
	fld	fa6, 14*8(sp)
	fld	fa7, 15*8(sp)
.if \kind == MSGSEND || \kind == MSGSEND_STRET
	ld	t0, 16*8(sp)
	ori	t0, t0, 2	// for the sake of instrumentations, remember it was the slowpath
.elseif \kind != METHOD_INVOKE && \kind != METHOD_INVOKE_STRET
.abort Unknown kind.
.endif

	.cfi_def_cfa sp, FRAME_SIZE
	ld	ra, FRAME_SIZE-8(sp)
	ld	s0, FRAME_SIZE-16(sp)
	.cfi_restore ra
	.cfi_restore s0
	addi	sp, sp, FRAME_SIZE
	.cfi_def_cfa_offset 0
.endm


// CacheLookup NORMAL|GETIMP|LOOKUP|STRET|STRET_LOOKUP <function> MissLabel Sel: IMP for Sel (a1, a2 stret) in class t0, kills t1..t6.
// Found: calls or returns IMP with t0 class, t1 IMP, t2 stret flag. Not found or restarted: jumps to MissLabel with t0 = class.

// CacheHit: t3 = matching bucket, t2 = sel read from it, t0 = isa
.macro CacheHit Mode, Sel
	// the imp load depends on the sel load through t2 (zero here) so rvwmo
	// cannot satisfy it early and pair a new sel with a stale imp
	xor	t2, t2, \Sel
	add	t1, t3, t2
	ld	t1, BUCKET_IMP(t1)
.if \Mode == NORMAL
	xor	t1, t1, t0	// decode imp, t2 is 0 for _objc_msgForward_impcache
	jr	t1
.elseif \Mode == STRET
	xor	t1, t1, t0	// decode imp
	li	t2, 1		// stret for _objc_msgForward_impcache
	jr	t1
.elseif \Mode == GETIMP
	mv	a0, t1
	beqz	a0, 9f		// don't decode a nil imp
	xor	a0, a0, t0
9:	ret			// return IMP
.elseif \Mode == LOOKUP
	xor	t1, t1, t0	// decode imp, t2 is 0
	ret			// return imp via t1
.elseif \Mode == STRET_LOOKUP
	xor	t1, t1, t0	// decode imp
	li	t2, 1		// stret for _objc_msgForward_impcache
	ret			// return imp via t1
.else
.abort oops
.endif
.endm

.macro CacheLookup Mode, Function, MissLabel, Sel
	// Restart protocol: a restart between LLookupStart\Function and LLookupEnd\Function resets PC to LLookupRecover\Function (cache miss).
	// GETIMP returns NULL there, the others need the receiver in a0 (a1 stret), the selector in a1 (a2 stret) and the isa in t0.
LLookupStart\Function:
	// Sel = SEL, t0 = isa
	ld	t4, CACHE(t0)		// t4 = mask|buckets
	srli	t6, t4, 48		// t6 = mask
	slli	t4, t4, 16
	srli	t4, t4, 16		// t4 = buckets
	and	t5, \Sel, t6		// t5 = _cmd & mask
	slli	t5, t5, 4
	add	t5, t4, t5		// t5 = first probed bucket
	slli	t6, t6, 4
	add	t6, t4, t6		// t6 = last bucket
	mv	t3, t5
					// do {
1:	ld	t2, BUCKET_SEL(t3)	//     sel = bucket->sel
	bne	t2, \Sel, 3f		//     if (sel != _cmd) scan more
2:	CacheHit \Mode, \Sel		//     hit: call or return imp
3:	beqz	t2, \MissLabel		//     if (sel == 0) goto Miss
	addi	t3, t3, -BUCKET_SIZE	//     bucket--
	bgeu	t3, t4, 1b		// } while (bucket >= buckets)

	// wrap-around: t4 = first bucket, t5 = first probed bucket, t6 = last bucket. A full cache (CACHE_ALLOW_FULL_UTILIZATION)
	// stops on circling back to the first probed bucket, which may be probed twice when it is the last entry.

	mv	t3, t6
					// do {
4:	ld	t2, BUCKET_SEL(t3)	//     sel = bucket->sel
	beq	t2, \Sel, 2b		//     if (sel == _cmd) goto hit
	beqz	t2, \MissLabel		//     if (sel == 0) goto Miss
	addi	t3, t3, -BUCKET_SIZE	//     bucket--
	bltu	t5, t3, 4b		// } while (bucket > first_probed)

LLookupEnd\Function:
LLookupRecover\Function:
	j	\MissLabel
.endm


// id objc_msgSend(id self, SEL _cmd, ...), IMP objc_msgLookup(id self, SEL _cmd, ...)
// objc_msgLookup ABI: IMP returned in t1, t0 and t2 must reach the IMP untouched

	.data
	.p2align 3
	.globl _objc_debug_taggedpointer_ext_classes
_objc_debug_taggedpointer_ext_classes:
	.fill 256, 8, 0

// Dispatch for split tagged pointers relies on the extended tag classes array immediately
// preceding the standard tag array. The .alt_entry directive keeps them together.
	.globl _objc_debug_taggedpointer_classes
	.alt_entry _objc_debug_taggedpointer_classes
_objc_debug_taggedpointer_classes:
	.fill 16, 8, 0

// Look up the class for a tagged pointer in Obj, placing it in t0.
.macro GetTaggedClass Obj
	andi	t4, \Obj, 7		// t4 = small tag
	srai	t5, \Obj, 55		// t5 = large tag with 1s filling the top (because bit 63 is 1 on a tagged pointer)
	li	t6, 7
	bne	t4, t6, 7f		// tag == 7 means an extended tag
	mv	t4, t5			// t4 = index in tagged pointer classes array, negative for extended tags
					// The extended tag array sits right before the basic tag array, so srai's sign extension
					// gives extended_tag - 256, the correct index in the extended tagged pointer classes array.
7:
	// t0 = _objc_debug_taggedpointer_classes[t4]
	lla	t6, _objc_debug_taggedpointer_classes
	slli	t4, t4, 3
	add	t6, t6, t4
	ld	t0, 0(t6)
.endm

.macro ZeroReturn
	// a0 is already zero
	li	a1, 0
	fmv.d.x	fa0, zero
	fmv.d.x	fa1, zero
.endm


	ENTRY _objc_msgSend

	blez	a0, LNilOrTagged	// nil check and tagged pointer check (MSB tagged pointer looks negative)
	ld	t2, 0(a0)		// t2 = isa
	ExtractISA t0, t2		// t0 = class
LGetIsaDone:
	// calls imp or objc_msgSend_uncached
	CacheLookup NORMAL, _objc_msgSend, __objc_msgSend_uncached, a1

LNilOrTagged:
	beqz	a0, LReturnZero		// nil check
	GetTaggedClass a0
	j	LGetIsaDone

LReturnZero:
	ZeroReturn
	ret

	END_ENTRY _objc_msgSend


	ENTRY _objc_msgLookup

	blez	a0, LLookup_NilOrTagged	// nil check and tagged pointer check (MSB tagged pointer looks negative)
	ld	t2, 0(a0)		// t2 = isa
	ExtractISA t0, t2		// t0 = class
LLookup_GetIsaDone:
	// returns imp
	CacheLookup LOOKUP, _objc_msgLookup, __objc_msgLookup_uncached, a1

LLookup_NilOrTagged:
	beqz	a0, LLookup_Nil		// nil check
	GetTaggedClass a0
	j	LLookup_GetIsaDone

LLookup_Nil:
	lla	t1, __objc_msgNil
	li	t2, 0
	ret

	END_ENTRY _objc_msgLookup


	STATIC_ENTRY __objc_msgNil

	ZeroReturn
	ret

	END_ENTRY __objc_msgNil


	ENTRY _objc_msgSendSuper

	ld	t0, CLASS(a0)		// t0 = class
	ld	a0, RECEIVER(a0)	// a0 = real receiver
	j	L_objc_msgSendSuper2_body

	END_ENTRY _objc_msgSendSuper

	// no _objc_msgLookupSuper

	ENTRY _objc_msgSendSuper2

	ld	t0, CLASS(a0)		// t0 = class
	ld	a0, RECEIVER(a0)	// a0 = real receiver
	ld	t0, SUPERCLASS(t0)	// t0 = class->superclass
L_objc_msgSendSuper2_body:
	CacheLookup NORMAL, _objc_msgSendSuper2, __objc_msgSend_uncached, a1

	END_ENTRY _objc_msgSendSuper2


	ENTRY _objc_msgLookupSuper2

	ld	t0, CLASS(a0)		// t0 = class
	ld	a0, RECEIVER(a0)	// a0 = real receiver
	ld	t0, SUPERCLASS(t0)	// t0 = class->superclass
	CacheLookup LOOKUP, _objc_msgLookupSuper2, __objc_msgLookup_uncached, a1

	END_ENTRY _objc_msgLookupSuper2


// void objc_msgSend_stret(void *st_addr, id self, SEL _cmd, ...)
// riscv returns wide structs via a hidden a0 pointer, moving self/_cmd to a1/a2 like x86_64, so clang calls the stret messengers.

	ENTRY _objc_msgSend_stret

	blez	a1, LNilOrTagged_stret	// nil check and tagged pointer check
	ld	t2, 0(a1)		// t2 = isa
	ExtractISA t0, t2		// t0 = class
LGetIsaDone_stret:
	// calls imp or objc_msgSend_stret_uncached
	CacheLookup STRET, _objc_msgSend_stret, __objc_msgSend_stret_uncached, a2

LNilOrTagged_stret:
	beqz	a1, LReturnZero_stret	// nil check
	GetTaggedClass a1
	j	LGetIsaDone_stret

LReturnZero_stret:
	// the caller zeroes the struct when it needs to
	ret

	END_ENTRY _objc_msgSend_stret


	ENTRY _objc_msgLookup_stret

	blez	a1, LLookup_NilOrTagged_stret
	ld	t2, 0(a1)		// t2 = isa
	ExtractISA t0, t2		// t0 = class
LLookup_GetIsaDone_stret:
	// returns imp
	CacheLookup STRET_LOOKUP, _objc_msgLookup_stret, __objc_msgLookup_stret_uncached, a2

LLookup_NilOrTagged_stret:
	beqz	a1, LLookup_Nil_stret	// nil check
	GetTaggedClass a1
	j	LLookup_GetIsaDone_stret

LLookup_Nil_stret:
	lla	t1, __objc_msgNil_stret
	li	t2, 1
	ret

	END_ENTRY _objc_msgLookup_stret


	STATIC_ENTRY __objc_msgNil_stret

	ret

	END_ENTRY __objc_msgNil_stret


	ENTRY _objc_msgSendSuper_stret

	ld	t0, CLASS(a1)		// t0 = class
	ld	a1, RECEIVER(a1)	// a1 = real receiver
	j	L_objc_msgSendSuper2_stret_body

	END_ENTRY _objc_msgSendSuper_stret


	ENTRY _objc_msgSendSuper2_stret

	ld	t0, CLASS(a1)		// t0 = class
	ld	a1, RECEIVER(a1)	// a1 = real receiver
	ld	t0, SUPERCLASS(t0)	// t0 = class->superclass
L_objc_msgSendSuper2_stret_body:
	CacheLookup STRET, _objc_msgSendSuper2_stret, __objc_msgSend_stret_uncached, a2

	END_ENTRY _objc_msgSendSuper2_stret


	ENTRY _objc_msgLookupSuper2_stret

	ld	t0, CLASS(a1)		// t0 = class
	ld	a1, RECEIVER(a1)	// a1 = real receiver
	ld	t0, SUPERCLASS(t0)	// t0 = class->superclass
	CacheLookup STRET_LOOKUP, _objc_msgLookupSuper2_stret, __objc_msgLookup_stret_uncached, a2

	END_ENTRY _objc_msgLookupSuper2_stret


.macro MethodTableLookup kind

	SAVE_REGS \kind

	// lookUpImpOrForward(obj, sel, cls, LOOKUP_INITIALIZE | LOOKUP_RESOLVER)
.if \kind == MSGSEND
	// receiver and selector already in a0 and a1
.else
	mv	a0, a1
	mv	a1, a2
.endif
	mv	a2, t0
	li	a3, 3
	call	_lookUpImpOrForward

	// IMP in a0
	mv	t1, a0

	RESTORE_REGS \kind

.endm

	STATIC_ENTRY __objc_msgSend_uncached

	// THIS IS NOT A CALLABLE C FUNCTION
	// Out-of-band t0 is the class to search

	MethodTableLookup MSGSEND
	li	t2, 0		// not stret, for _objc_msgForward_impcache
	jr	t1

	END_ENTRY __objc_msgSend_uncached


	STATIC_ENTRY __objc_msgSend_stret_uncached

	// THIS IS NOT A CALLABLE C FUNCTION
	// Out-of-band t0 is the class to search

	MethodTableLookup MSGSEND_STRET
	li	t2, 1		// stret, for _objc_msgForward_impcache
	jr	t1

	END_ENTRY __objc_msgSend_stret_uncached


	STATIC_ENTRY __objc_msgLookup_uncached

	// THIS IS NOT A CALLABLE C FUNCTION
	// Out-of-band t0 is the class to search

	MethodTableLookup MSGSEND
	li	t2, 0
	ret

	END_ENTRY __objc_msgLookup_uncached


	STATIC_ENTRY __objc_msgLookup_stret_uncached

	// THIS IS NOT A CALLABLE C FUNCTION
	// Out-of-band t0 is the class to search

	MethodTableLookup MSGSEND_STRET
	li	t2, 1
	ret

	END_ENTRY __objc_msgLookup_stret_uncached


	STATIC_ENTRY _cache_getImp

	mv	t0, a0
	CacheLookup GETIMP, _cache_getImp, LGetImpMissDynamic, a1

LGetImpMissDynamic:
	li	a0, 0
	ret

	END_ENTRY _cache_getImp


// id _objc_msgForward(id self, SEL _cmd,...): _objc_msgForward(_stret) are the externally-callable versions,
// _objc_msgForward_impcache is the function pointer actually stored in method caches.

	STATIC_ENTRY __objc_msgForward_impcache

	// THIS IS NOT A CALLABLE C FUNCTION
	// Out-of-band t2 is nonzero for stret, zero otherwise
	bnez	t2, LForwardStret
	j	__objc_msgForward
LForwardStret:
	j	__objc_msgForward_stret

	END_ENTRY __objc_msgForward_impcache


	ENTRY __objc_msgForward

	lla	t1, __objc_forward_handler
	ld	t1, 0(t1)
	jr	t1

	END_ENTRY __objc_msgForward


	ENTRY __objc_msgForward_stret

	lla	t1, __objc_forward_stret_handler
	ld	t1, 0(t1)
	jr	t1

	END_ENTRY __objc_msgForward_stret


	ENTRY _objc_msgSend_noarg
	j	_objc_msgSend
	END_ENTRY _objc_msgSend_noarg

	ENTRY _objc_msgSend_debug
	j	_objc_msgSend
	END_ENTRY _objc_msgSend_debug

	ENTRY _objc_msgSendSuper2_debug
	j	_objc_msgSendSuper2
	END_ENTRY _objc_msgSendSuper2_debug

	ENTRY _objc_msgSend_stret_debug
	j	_objc_msgSend_stret
	END_ENTRY _objc_msgSend_stret_debug

	ENTRY _objc_msgSendSuper2_stret_debug
	j	_objc_msgSendSuper2_stret
	END_ENTRY _objc_msgSendSuper2_stret_debug


	ENTRY _method_invoke

	// See if this is a small method.
	andi	t1, a1, 1
	bnez	t1, L_method_invoke_small

	// We can directly load the IMP from big methods.
	// a1 is method triplet instead of SEL
	ld	t1, METHOD_IMP(a1)
	ld	a1, METHOD_NAME(a1)
	jr	t1

L_method_invoke_small:
	// Small methods require a call to handle swizzling.
	SAVE_REGS METHOD_INVOKE
	mv	a0, a1
	call	__method_getImplementationAndName
	// IMPAndSEL comes back in a0 and a1
	mv	t1, a0
	mv	t0, a1
	RESTORE_REGS METHOD_INVOKE
	mv	a1, t0
	jr	t1

	END_ENTRY _method_invoke


	ENTRY _method_invoke_stret

	// See if this is a small method.
	andi	t1, a2, 1
	bnez	t1, L_method_invoke_stret_small

	// We can directly load the IMP from big methods.
	// a2 is method triplet instead of SEL
	ld	t1, METHOD_IMP(a2)
	ld	a2, METHOD_NAME(a2)
	jr	t1

L_method_invoke_stret_small:
	// Small methods require a call to handle swizzling.
	SAVE_REGS METHOD_INVOKE_STRET
	mv	a0, a2
	call	__method_getImplementationAndName
	// IMPAndSEL comes back in a0 and a1
	mv	t1, a0
	mv	t0, a1
	RESTORE_REGS METHOD_INVOKE_STRET
	mv	a2, t0
	jr	t1

	END_ENTRY _method_invoke_stret

#endif

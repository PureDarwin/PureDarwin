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
/********************************************************************
 * 
 *  objc-msg-arm64.s - ARM64 code to support objc messaging
 *
 ********************************************************************/

#ifdef __arm64__

#include <arm/arch.h>
#include "isa.h"
#include "objc-config.h"
#include "arm64-asm.h"

#if TARGET_OS_IPHONE && __LP64__
	.section	__TEXT,__objc_methname,cstring_literals
l_MagicSelector: /* the shared cache builder knows about this value */
        .byte	0xf0, 0x9f, 0xa4, 0xaf, 0

	.section	__DATA,__objc_selrefs,literal_pointers,no_dead_strip
	.p2align	3
_MagicSelRef:
	.quad	l_MagicSelector
#endif

.data

// _objc_restartableRanges is used by method dispatch
// caching code to figure out whether any threads are actively 
// in the cache for dispatching.  The labels surround the asm code
// that do cache lookups.  The tables are zero-terminated.

.macro RestartableEntry
#if __LP64__
	.quad	LLookupStart$0
#else
	.long	LLookupStart$0
	.long	0
#endif
	.short	LLookupEnd$0 - LLookupStart$0
	.short	LLookupRecover$0 - LLookupStart$0
	.long	0
.endmacro

	.align 4
	.private_extern _objc_restartableRanges
_objc_restartableRanges:
	RestartableEntry _cache_getImp
	RestartableEntry _objc_msgSend
	RestartableEntry _objc_msgSendSuper2
	RestartableEntry _objc_msgLookup
	RestartableEntry _objc_msgLookupSuper2
	.fill	16, 1, 0


/* objc_super parameter to sendSuper */
#define RECEIVER         0
#define CLASS            __SIZEOF_POINTER__

/* Selected field offsets in class structure */
#define SUPERCLASS       __SIZEOF_POINTER__
#define CACHE            (2 * __SIZEOF_POINTER__)

/* Selected field offsets in method structure */
#define METHOD_NAME      0
#define METHOD_TYPES     __SIZEOF_POINTER__
#define METHOD_IMP       (2 * __SIZEOF_POINTER__)

#define BUCKET_SIZE      (2 * __SIZEOF_POINTER__)


/********************************************************************
 * GetClassFromIsa_p16 src, needs_auth, auth_address
 * src is a raw isa field. Sets p16 to the corresponding class pointer.
 * The raw isa might be an indexed isa to be decoded, or a
 * packed isa that needs to be masked.
 *
 * On exit:
 *   src is unchanged
 *   p16 is a class pointer
 *   x10 is clobbered
 ********************************************************************/

#if SUPPORT_INDEXED_ISA
	.align 3
	.globl _objc_indexed_classes
_objc_indexed_classes:
	.fill ISA_INDEX_COUNT, PTRSIZE, 0
#endif

.macro GetClassFromIsa_p16 src, needs_auth, auth_address /* note: auth_address is not required if !needs_auth */

#if SUPPORT_INDEXED_ISA
	// Indexed isa
	mov	p16, \src			// optimistically set dst = src
	tbz	p16, #ISA_INDEX_IS_NPI_BIT, 1f	// done if not non-pointer isa
	// isa in p16 is indexed
	adrp	x10, _objc_indexed_classes@PAGE
	add	x10, x10, _objc_indexed_classes@PAGEOFF
	ubfx	p16, p16, #ISA_INDEX_SHIFT, #ISA_INDEX_BITS  // extract index
	ldr	p16, [x10, p16, UXTP #PTRSHIFT]	// load class from array
1:

#elif __LP64__
.if \needs_auth == 0 // _cache_getImp takes an authed class already
	mov	p16, \src
.else
	// 64-bit packed isa
	ExtractISA p16, \src, \auth_address
.endif
#else
	// 32-bit raw isa
	mov	p16, \src

#endif

.endmacro


/********************************************************************
 * ENTRY functionName
 * STATIC_ENTRY functionName
 * END_ENTRY functionName
 ********************************************************************/

.macro ENTRY /* name */
	.text
	.align 5
	.globl    $0
$0:
.endmacro

.macro STATIC_ENTRY /*name*/
	.text
	.align 5
	.private_extern $0
$0:
.endmacro

.macro END_ENTRY /* name */
LExit$0:
.endmacro


/********************************************************************
 * UNWIND name, flags
 * Unwind info generation	
 ********************************************************************/
.macro UNWIND
	.section __LD,__compact_unwind,regular,debug
	PTR $0
	.set  LUnwind$0, LExit$0 - $0
	.long LUnwind$0
	.long $1
	PTR 0	 /* no personality */
	PTR 0  /* no LSDA */
	.text
.endmacro

#define NoFrame 0x02000000  // no frame, no SP adjustment
#define FrameWithNoSaves 0x04000000  // frame, no non-volatile saves


#define MSGSEND 100
#define METHOD_INVOKE 101

//////////////////////////////////////////////////////////////////////
//
// SAVE_REGS
//
// Create a stack frame and save all argument registers in preparation
// for a function call.
//////////////////////////////////////////////////////////////////////

.macro SAVE_REGS kind

    // push frame
    SignLR
    stp    fp, lr, [sp, #-16]!
    mov    fp, sp

    // save parameter registers: x0..x8, q0..q7
    sub    sp, sp,  #(10*8 + 8*16)
    stp    q0, q1,  [sp, #(0*16)]
    stp    q2, q3,  [sp, #(2*16)]
    stp    q4, q5,  [sp, #(4*16)]
    stp    q6, q7,  [sp, #(6*16)]
    stp    x0, x1,  [sp, #(8*16+0*8)]
    stp    x2, x3,  [sp, #(8*16+2*8)]
    stp    x4, x5,  [sp, #(8*16+4*8)]
    stp    x6, x7,  [sp, #(8*16+6*8)]
.if \kind == MSGSEND
    stp    x8, x15, [sp, #(8*16+8*8)]
    mov    x16, x15 // stashed by CacheLookup, restore to x16
.elseif \kind == METHOD_INVOKE
    str    x8,      [sp, #(8*16+8*8)]
.else
.abort Unknown kind.
.endif

.endmacro


//////////////////////////////////////////////////////////////////////
//
// RESTORE_REGS
//
// Restore all argument registers and pop the stack frame created by
// SAVE_REGS.
//////////////////////////////////////////////////////////////////////

.macro RESTORE_REGS kind

    ldp    q0, q1,  [sp, #(0*16)]
    ldp    q2, q3,  [sp, #(2*16)]
    ldp    q4, q5,  [sp, #(4*16)]
    ldp    q6, q7,  [sp, #(6*16)]
    ldp    x0, x1,  [sp, #(8*16+0*8)]
    ldp    x2, x3,  [sp, #(8*16+2*8)]
    ldp    x4, x5,  [sp, #(8*16+4*8)]
    ldp    x6, x7,  [sp, #(8*16+6*8)]
.if \kind == MSGSEND
    ldp    x8, x16, [sp, #(8*16+8*8)]
    orr    x16, x16, #2  // for the sake of instrumentations, remember it was the slowpath
.elseif \kind == METHOD_INVOKE
    ldr    x8,      [sp, #(8*16+8*8)]
.else
.abort Unknown kind.
.endif

    mov    sp, fp
    ldp    fp, lr, [sp], #16
    AuthenticateLR

.endmacro


/********************************************************************
 *
 * CacheLookup NORMAL|GETIMP|LOOKUP <function> MissLabelDynamic MissLabelConstant
 *
 * MissLabelConstant is only used for the GETIMP variant.
 *
 * Locate the implementation for a selector in a class method cache.
 *
 * When this is used in a function that doesn't hold the runtime lock,
 * this represents the critical section that may access dead memory.
 * If the kernel causes one of these functions to go down the recovery
 * path, we pretend the lookup failed by jumping the JumpMiss branch.
 *
 * Takes:
 *	 x1 = selector
 *	 x16 = class to be searched
 *
 * Kills:
 * 	 x9,x10,x11,x12,x13,x15,x17
 *
 * Untouched:
 * 	 x14
 *
 * On exit: (found) calls or returns IMP
 *                  with x16 = class, x17 = IMP
 *                  In LOOKUP mode, the two low bits are set to 0x3
 *                  if we hit a constant cache (used in objc_trace)
 *          (not found) jumps to LCacheMiss
 *                  with x15 = class
 *                  For constant caches in LOOKUP mode, the low bit
 *                  of x16 is set to 0x1 to indicate we had to fallback.
 *          In addition, when LCacheMiss is __objc_msgSend_uncached or
 *          __objc_msgLookup_uncached, 0x2 will be set in x16
 *          to remember we took the slowpath.
 *          So the two low bits of x16 on exit mean:
 *            0: dynamic hit
 *            1: fallback to the parent class, when there is a preoptimized cache
 *            2: slowpath
 *            3: preoptimized cache hit
 *
 ********************************************************************/

#define NORMAL 0
#define GETIMP 1
#define LOOKUP 2

// CacheHit: x17 = cached IMP, x10 = address of buckets, x1 = SEL, x16 = isa
.macro CacheHit
.if $0 == NORMAL
	TailCallCachedImp x17, x10, x1, x16	// authenticate and call imp
.elseif $0 == GETIMP
	mov	p0, p17
	cbz	p0, 9f			// don't ptrauth a nil imp
	AuthAndResignAsIMP x0, x10, x1, x16	// authenticate imp and re-sign as IMP
9:	ret				// return IMP
.elseif $0 == LOOKUP
	// No nil check for ptrauth: the caller would crash anyway when they
	// jump to a nil IMP. We don't care if that jump also fails ptrauth.
	AuthAndResignAsIMP x17, x10, x1, x16	// authenticate imp and re-sign as IMP
	cmp	x16, x15
	cinc	x16, x16, ne			// x16 += 1 when x15 != x16 (for instrumentation ; fallback to the parent class)
	ret				// return imp via x17
.else
.abort oops
.endif
.endmacro

.macro CacheLookup Mode, Function, MissLabelDynamic, MissLabelConstant
	//
	// Restart protocol:
	//
	//   As soon as we're past the LLookupStart\Function label we may have
	//   loaded an invalid cache pointer or mask.
	//
	//   When task_restartable_ranges_synchronize() is called,
	//   (or when a signal hits us) before we're past LLookupEnd\Function,
	//   then our PC will be reset to LLookupRecover\Function which forcefully
	//   jumps to the cache-miss codepath which have the following
	//   requirements:
	//
	//   GETIMP:
	//     The cache-miss is just returning NULL (setting x0 to 0)
	//
	//   NORMAL and LOOKUP:
	//   - x0 contains the receiver
	//   - x1 contains the selector
	//   - x16 contains the isa
	//   - other registers are set as per calling conventions
	//

	mov	x15, x16			// stash the original isa
LLookupStart\Function:
	// p1 = SEL, p16 = isa
#if CACHE_MASK_STORAGE == CACHE_MASK_STORAGE_HIGH_16_BIG_ADDRS
	ldr	p10, [x16, #CACHE]				// p10 = mask|buckets
	lsr	p11, p10, #48			// p11 = mask
	and	p10, p10, #0xffffffffffff	// p10 = buckets
	and	w12, w1, w11			// x12 = _cmd & mask
#elif CACHE_MASK_STORAGE == CACHE_MASK_STORAGE_HIGH_16
	ldr	p11, [x16, #CACHE]			// p11 = mask|buckets
#if CONFIG_USE_PREOPT_CACHES
#if __has_feature(ptrauth_calls)
	tbnz	p11, #0, LLookupPreopt\Function
	and	p10, p11, #0x0000ffffffffffff	// p10 = buckets
#else
	and	p10, p11, #0x0000fffffffffffe	// p10 = buckets
	tbnz	p11, #0, LLookupPreopt\Function
#endif
	eor	p12, p1, p1, LSR #7
	and	p12, p12, p11, LSR #48		// x12 = (_cmd ^ (_cmd >> 7)) & mask
#else
	and	p10, p11, #0x0000ffffffffffff	// p10 = buckets
	and	p12, p1, p11, LSR #48		// x12 = _cmd & mask
#endif // CONFIG_USE_PREOPT_CACHES
#elif CACHE_MASK_STORAGE == CACHE_MASK_STORAGE_LOW_4
	ldr	p11, [x16, #CACHE]				// p11 = mask|buckets
	and	p10, p11, #~0xf			// p10 = buckets
	and	p11, p11, #0xf			// p11 = maskShift
	mov	p12, #0xffff
	lsr	p11, p12, p11			// p11 = mask = 0xffff >> p11
	and	p12, p1, p11			// x12 = _cmd & mask
#else
#error Unsupported cache mask storage for ARM64.
#endif

	add	p13, p10, p12, LSL #(1+PTRSHIFT)
						// p13 = buckets + ((_cmd & mask) << (1+PTRSHIFT))

						// do {
1:	ldp	p17, p9, [x13], #-BUCKET_SIZE	//     {imp, sel} = *bucket--
	cmp	p9, p1				//     if (sel != _cmd) {
	b.ne	3f				//         scan more
						//     } else {
2:	CacheHit \Mode				// hit:    call or return imp
						//     }
3:	cbz	p9, \MissLabelDynamic		//     if (sel == 0) goto Miss;
	cmp	p13, p10			// } while (bucket >= buckets)
	b.hs	1b

	// wrap-around:
	//   p10 = first bucket
	//   p11 = mask (and maybe other bits on LP64)
	//   p12 = _cmd & mask
	//
	// A full cache can happen with CACHE_ALLOW_FULL_UTILIZATION.
	// So stop when we circle back to the first probed bucket
	// rather than when hitting the first bucket again.
	//
	// Note that we might probe the initial bucket twice
	// when the first probed slot is the last entry.


#if CACHE_MASK_STORAGE == CACHE_MASK_STORAGE_HIGH_16_BIG_ADDRS
	add	p13, p10, w11, UXTW #(1+PTRSHIFT)
						// p13 = buckets + (mask << 1+PTRSHIFT)
#elif CACHE_MASK_STORAGE == CACHE_MASK_STORAGE_HIGH_16
	add	p13, p10, p11, LSR #(48 - (1+PTRSHIFT))
						// p13 = buckets + (mask << 1+PTRSHIFT)
						// see comment about maskZeroBits
#elif CACHE_MASK_STORAGE == CACHE_MASK_STORAGE_LOW_4
	add	p13, p10, p11, LSL #(1+PTRSHIFT)
						// p13 = buckets + (mask << 1+PTRSHIFT)
#else
#error Unsupported cache mask storage for ARM64.
#endif
	add	p12, p10, p12, LSL #(1+PTRSHIFT)
						// p12 = first probed bucket

						// do {
4:	ldp	p17, p9, [x13], #-BUCKET_SIZE	//     {imp, sel} = *bucket--
	cmp	p9, p1				//     if (sel == _cmd)
	b.eq	2b				//         goto hit
	cmp	p9, #0				// } while (sel != 0 &&
	ccmp	p13, p12, #0, ne		//     bucket > first_probed)
	b.hi	4b

LLookupEnd\Function:
LLookupRecover\Function:
	b	\MissLabelDynamic

#if CONFIG_USE_PREOPT_CACHES
#if CACHE_MASK_STORAGE != CACHE_MASK_STORAGE_HIGH_16
#error config unsupported
#endif
LLookupPreopt\Function:
#if __has_feature(ptrauth_calls)
	and	p10, p11, #0x007ffffffffffffe	// p10 = buckets
	autdb	x10, x16			// auth as early as possible
#endif

	// x12 = (_cmd - first_shared_cache_sel)
	adrp	x9, _MagicSelRef@PAGE
	ldr	p9, [x9, _MagicSelRef@PAGEOFF]
	sub	p12, p1, p9

	// w9  = ((_cmd - first_shared_cache_sel) >> hash_shift & hash_mask)
#if __has_feature(ptrauth_calls)
	// bits 63..60 of x11 are the number of bits in hash_mask
	// bits 59..55 of x11 is hash_shift

	lsr	x17, x11, #55			// w17 = (hash_shift, ...)
	lsr	w9, w12, w17			// >>= shift

	lsr	x17, x11, #60			// w17 = mask_bits
	mov	x11, #0x7fff
	lsr	x11, x11, x17			// p11 = mask (0x7fff >> mask_bits)
	and	x9, x9, x11			// &= mask
#else
	// bits 63..53 of x11 is hash_mask
	// bits 52..48 of x11 is hash_shift
	lsr	x17, x11, #48			// w17 = (hash_shift, hash_mask)
	lsr	w9, w12, w17			// >>= shift
	and	x9, x9, x11, LSR #53		// &=  mask
#endif

	ldr	x17, [x10, x9, LSL #3]		// x17 == sel_offs | (imp_offs << 32)
	cmp	x12, w17, uxtw

.if \Mode == GETIMP
	b.ne	\MissLabelConstant		// cache miss
	sub	x0, x16, x17, LSR #32		// imp = isa - imp_offs
	SignAsImp x0
	ret
.else
	b.ne	5f				// cache miss
	sub	x17, x16, x17, LSR #32		// imp = isa - imp_offs
.if \Mode == NORMAL
	br	x17
.elseif \Mode == LOOKUP
	orr x16, x16, #3 // for instrumentation, note that we hit a constant cache
	SignAsImp x17
	ret
.else
.abort  unhandled mode \Mode
.endif

5:	ldursw	x9, [x10, #-8]			// offset -8 is the fallback offset
	add	x16, x16, x9			// compute the fallback isa
	b	LLookupStart\Function		// lookup again with a new isa
.endif
#endif // CONFIG_USE_PREOPT_CACHES

.endmacro


/********************************************************************
 *
 * id objc_msgSend(id self, SEL _cmd, ...);
 * IMP objc_msgLookup(id self, SEL _cmd, ...);
 * 
 * objc_msgLookup ABI:
 * IMP returned in x17
 * x16 reserved for our use but not used
 *
 ********************************************************************/

#if SUPPORT_TAGGED_POINTERS
	.data
	.align 3
	.globl _objc_debug_taggedpointer_ext_classes
_objc_debug_taggedpointer_ext_classes:
	.fill 256, 8, 0

// Dispatch for split tagged pointers take advantage of the fact that
// the extended tag classes array immediately precedes the standard
// tag array. The .alt_entry directive ensures that the two stay
// together. This is harmless when using non-split tagged pointers.
	.globl _objc_debug_taggedpointer_classes
	.alt_entry _objc_debug_taggedpointer_classes
_objc_debug_taggedpointer_classes:
	.fill 16, 8, 0

// Look up the class for a tagged pointer in x0, placing it in x16.
.macro GetTaggedClass

	and	x10, x0, #0x7		// x10 = small tag
	asr	x11, x0, #55		// x11 = large tag with 1s filling the top (because bit 63 is 1 on a tagged pointer)
	cmp	x10, #7		// tag == 7?
	csel	x12, x11, x10, eq	// x12 = index in tagged pointer classes array, negative for extended tags.
					// The extended tag array is placed immediately before the basic tag array
					// so this looks into the right place either way. The sign extension done
					// by the asr instruction produces the value extended_tag - 256, which produces
					// the correct index in the extended tagged pointer classes array.

	// x16 = _objc_debug_taggedpointer_classes[x12]
	adrp	x10, _objc_debug_taggedpointer_classes@PAGE
	add	x10, x10, _objc_debug_taggedpointer_classes@PAGEOFF
	ldr	x16, [x10, x12, LSL #3]

.endmacro
#endif

	ENTRY _objc_msgSend
	UNWIND _objc_msgSend, NoFrame

	cmp	p0, #0			// nil check and tagged pointer check
#if SUPPORT_TAGGED_POINTERS
	b.le	LNilOrTagged		//  (MSB tagged pointer looks negative)
#else
	b.eq	LReturnZero
#endif
	ldr	p13, [x0]		// p13 = isa
	GetClassFromIsa_p16 p13, 1, x0	// p16 = class
LGetIsaDone:
	// calls imp or objc_msgSend_uncached
	CacheLookup NORMAL, _objc_msgSend, __objc_msgSend_uncached

#if SUPPORT_TAGGED_POINTERS
LNilOrTagged:
	b.eq	LReturnZero		// nil check
	GetTaggedClass
	b	LGetIsaDone
// SUPPORT_TAGGED_POINTERS
#endif

LReturnZero:
	// x0 is already zero
	mov	x1, #0
	movi	d0, #0
	movi	d1, #0
	movi	d2, #0
	movi	d3, #0
	ret

	END_ENTRY _objc_msgSend


	ENTRY _objc_msgLookup
	UNWIND _objc_msgLookup, NoFrame
	cmp	p0, #0			// nil check and tagged pointer check
#if SUPPORT_TAGGED_POINTERS
	b.le	LLookup_NilOrTagged	//  (MSB tagged pointer looks negative)
#else
	b.eq	LLookup_Nil
#endif
	ldr	p13, [x0]		// p13 = isa
	GetClassFromIsa_p16 p13, 1, x0	// p16 = class
LLookup_GetIsaDone:
	// returns imp
	CacheLookup LOOKUP, _objc_msgLookup, __objc_msgLookup_uncached

#if SUPPORT_TAGGED_POINTERS
LLookup_NilOrTagged:
	b.eq	LLookup_Nil	// nil check
	GetTaggedClass
	b	LLookup_GetIsaDone
// SUPPORT_TAGGED_POINTERS
#endif

LLookup_Nil:
	adr	x17, __objc_msgNil
	SignAsImp x17
	ret

	END_ENTRY _objc_msgLookup

	
	STATIC_ENTRY __objc_msgNil

	// x0 is already zero
	mov	x1, #0
	movi	d0, #0
	movi	d1, #0
	movi	d2, #0
	movi	d3, #0
	ret
	
	END_ENTRY __objc_msgNil


	ENTRY _objc_msgSendSuper
	UNWIND _objc_msgSendSuper, NoFrame

	ldp	p0, p16, [x0]		// p0 = real receiver, p16 = class
	b L_objc_msgSendSuper2_body

	END_ENTRY _objc_msgSendSuper

	// no _objc_msgLookupSuper

	ENTRY _objc_msgSendSuper2
	UNWIND _objc_msgSendSuper2, NoFrame

#if __has_feature(ptrauth_calls)
	ldp	x0, x17, [x0]		// x0 = real receiver, x17 = class
	add	x17, x17, #SUPERCLASS	// x17 = &class->superclass
	ldr	x16, [x17]		// x16 = class->superclass
	AuthISASuper x16, x17, ISA_SIGNING_DISCRIMINATOR_CLASS_SUPERCLASS
LMsgSendSuperResume:
#else
	ldp	p0, p16, [x0]		// p0 = real receiver, p16 = class
	ldr	p16, [x16, #SUPERCLASS]	// p16 = class->superclass
#endif
L_objc_msgSendSuper2_body:
	CacheLookup NORMAL, _objc_msgSendSuper2, __objc_msgSend_uncached

	END_ENTRY _objc_msgSendSuper2

	
	ENTRY _objc_msgLookupSuper2
	UNWIND _objc_msgLookupSuper2, NoFrame

#if __has_feature(ptrauth_calls)
	ldp	x0, x17, [x0]		// x0 = real receiver, x17 = class
	add	x17, x17, #SUPERCLASS	// x17 = &class->superclass
	ldr	x16, [x17]		// x16 = class->superclass
	AuthISASuper x16, x17, ISA_SIGNING_DISCRIMINATOR_CLASS_SUPERCLASS
LMsgLookupSuperResume:
#else
	ldp	p0, p16, [x0]		// p0 = real receiver, p16 = class
	ldr	p16, [x16, #SUPERCLASS]	// p16 = class->superclass
#endif
	CacheLookup LOOKUP, _objc_msgLookupSuper2, __objc_msgLookup_uncached

	END_ENTRY _objc_msgLookupSuper2


.macro MethodTableLookup
	
	SAVE_REGS MSGSEND

	// lookUpImpOrForward(obj, sel, cls, LOOKUP_INITIALIZE | LOOKUP_RESOLVER)
	// receiver and selector already in x0 and x1
	mov	x2, x16
	mov	x3, #3
	bl	_lookUpImpOrForward

	// IMP in x0
	mov	x17, x0

	RESTORE_REGS MSGSEND

.endmacro

	STATIC_ENTRY __objc_msgSend_uncached
	UNWIND __objc_msgSend_uncached, FrameWithNoSaves

	// THIS IS NOT A CALLABLE C FUNCTION
	// Out-of-band p15 is the class to search
	
	MethodTableLookup
	TailCallFunctionPointer x17

	END_ENTRY __objc_msgSend_uncached


	STATIC_ENTRY __objc_msgLookup_uncached
	UNWIND __objc_msgLookup_uncached, FrameWithNoSaves

	// THIS IS NOT A CALLABLE C FUNCTION
	// Out-of-band p15 is the class to search
	
	MethodTableLookup
	ret

	END_ENTRY __objc_msgLookup_uncached


	STATIC_ENTRY _cache_getImp

	GetClassFromIsa_p16 p0, 0
	CacheLookup GETIMP, _cache_getImp, LGetImpMissDynamic, LGetImpMissConstant

LGetImpMissDynamic:
	mov	p0, #0
	ret

LGetImpMissConstant:
	mov	p0, p2
	ret

	END_ENTRY _cache_getImp


/********************************************************************
*
* id _objc_msgForward(id self, SEL _cmd,...);
*
* _objc_msgForward is the externally-callable
*   function returned by things like method_getImplementation().
* _objc_msgForward_impcache is the function pointer actually stored in
*   method caches.
*
********************************************************************/

	STATIC_ENTRY __objc_msgForward_impcache

	// No stret specialization.
	b	__objc_msgForward

	END_ENTRY __objc_msgForward_impcache

	
	ENTRY __objc_msgForward

	adrp	x17, __objc_forward_handler@PAGE
	ldr	p17, [x17, __objc_forward_handler@PAGEOFF]
	TailCallFunctionPointer x17
	
	END_ENTRY __objc_msgForward
	
	
	ENTRY _objc_msgSend_noarg
	b	_objc_msgSend
	END_ENTRY _objc_msgSend_noarg

	ENTRY _objc_msgSend_debug
	b	_objc_msgSend
	END_ENTRY _objc_msgSend_debug

	ENTRY _objc_msgSendSuper2_debug
	b	_objc_msgSendSuper2
	END_ENTRY _objc_msgSendSuper2_debug

	
	ENTRY _method_invoke

	// See if this is a small method.
	tbnz	p1, #0, L_method_invoke_small

	// We can directly load the IMP from big methods.
	// x1 is method triplet instead of SEL
	add	p16, p1, #METHOD_IMP
	ldr	p17, [x16]
	ldr	p1, [x1, #METHOD_NAME]
	TailCallMethodListImp x17, x16

L_method_invoke_small:
	// Small methods require a call to handle swizzling.
	SAVE_REGS METHOD_INVOKE
	mov	p0, p1
	bl	__method_getImplementationAndName
	// ARM64_32 packs both return values into x0, with SEL in the high bits and IMP in the low.
	// ARM64 just returns them in x0 and x1.
	mov	x17, x0
#if __LP64__
	mov	x16, x1
#endif
	RESTORE_REGS METHOD_INVOKE
#if __LP64__
	mov	x1, x16
#else
	lsr	x1, x17, #32
	mov	w17, w17
#endif
	TailCallFunctionPointer x17

	END_ENTRY _method_invoke

#endif

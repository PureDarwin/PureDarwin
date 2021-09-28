/*
 * Copyright (c) 2020 Apple Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
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

/* ASM Macro helpers */
#if defined(__ASSEMBLER__)

.macro ARM64_STACK_PROLOG
#if __has_feature(ptrauth_returns)
	pacibsp
#endif
.endmacro

.macro ARM64_STACK_EPILOG
#if __has_feature(ptrauth_returns)
	retab
#else
	ret
#endif
.endmacro

#define PUSH_FRAME			\
	stp fp, lr, [sp, #-16]!		%% \
	mov fp, sp			%%

#define POP_FRAME			\
	mov sp, fp			%% \
	ldp fp, lr, [sp], #16		%%
#endif /* ASSEMBLER */

/* Offsets of the various register states inside of the mcontext data */
#define MCONTEXT_OFFSET_X0 16

#define MCONTEXT_OFFSET_X19_X20 168
#define MCONTEXT_OFFSET_X21_X22 184
#define MCONTEXT_OFFSET_X23_X24 200

#define MCONTEXT_OFFSET_X25_X26 216
#define MCONTEXT_OFFSET_X27_X28 232

#define MCONTEXT_OFFSET_FP_LR 248
#define MCONTEXT_OFFSET_SP 264
#define MCONTEXT_OFFSET_FLAGS 284

#define MCONTEXT_OFFSET_D8 424
#define MCONTEXT_OFFSET_D9 440
#define MCONTEXT_OFFSET_D10 456
#define MCONTEXT_OFFSET_D11 472
#define MCONTEXT_OFFSET_D12 488
#define MCONTEXT_OFFSET_D13 504
#define MCONTEXT_OFFSET_D14 520
#define MCONTEXT_OFFSET_D15 536

#if __has_feature(ptrauth_calls)
#define LR_SIGNED_WITH_IB 0x2 /* Copied from __DARWIN_ARM_THREAD_STATE64_FLAGS_IB_SIGNED_LR */
#define LR_SIGNED_WITH_IB_BIT 0x1
#endif

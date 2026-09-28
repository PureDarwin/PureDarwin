#if defined(__riscv) && __riscv_xlen == 64

#include <mach/vm_param.h>

// offset of block->invoke
#define BLOCK_INVOKE 16

// a trampoline is one 8 byte slot, an auipc to its data and a jump to the page header, so nothing may be compressed
// the header spans the data slots that hold TrampolineBlockPageGroup's four words
#define TRAMPOLINE_HEADER_SIZE 32
#define TRAMPOLINE_SLOTS ((PAGE_MAX_SIZE - TRAMPOLINE_HEADER_SIZE) / 8)

// auipc immediate that reaches n pages back
#define PAGES_BACK(n) (0x100000 - (((n) * PAGE_MAX_SIZE) >> 12))

	.option norvc

.text
.globl __objc_blockTrampolineImpl
.globl __objc_blockTrampolineStart
.globl __objc_blockTrampolineLast

.p2align PAGE_MAX_SHIFT
__objc_blockTrampolineImpl:
L_objc_blockTrampolineImpl:
	// a0 == self, ra == original return address
	// t1 == address of called trampoline's data (2 pages before its code)
	mv	a1, a0			// _cmd = self
	ld	a0, 0(t1)		// self = block object
	ld	t1, BLOCK_INVOKE(a0)	// t1 = block->invoke
	jr	t1

	// pad up to the header size
	nop
	nop
	nop
	nop

.macro TrampolineEntry
	// load address of trampoline data (two pages before this instruction)
	auipc	t1, PAGES_BACK(2)
	j	L_objc_blockTrampolineImpl
.endm

.p2align 5
__objc_blockTrampolineStart:
	.rept TRAMPOLINE_SLOTS - 1
	TrampolineEntry
	.endr
__objc_blockTrampolineLast:
	TrampolineEntry


// riscv returns large structs through a pointer in a0, so these shift self to a1
.globl __objc_blockTrampolineImpl_stret
.globl __objc_blockTrampolineStart_stret
.globl __objc_blockTrampolineLast_stret

.p2align PAGE_MAX_SHIFT
__objc_blockTrampolineImpl_stret:
L_objc_blockTrampolineImpl_stret:
	// a0 == address of the return value's space, left alone, a1 == self
	// t1 == address of called trampoline's data (3 pages before its code)
	mv	a2, a1			// _cmd = self
	ld	a1, 0(t1)		// self = block object
	ld	t1, BLOCK_INVOKE(a1)	// t1 = block->invoke
	jr	t1

	// pad up to the header size
	nop
	nop
	nop
	nop

.macro TrampolineEntry_stret
	// load address of trampoline data (three pages before this instruction)
	auipc	t1, PAGES_BACK(3)
	j	L_objc_blockTrampolineImpl_stret
.endm

.p2align 5
__objc_blockTrampolineStart_stret:
	.rept TRAMPOLINE_SLOTS - 1
	TrampolineEntry_stret
	.endr
__objc_blockTrampolineLast_stret:
	TrampolineEntry_stret

#endif

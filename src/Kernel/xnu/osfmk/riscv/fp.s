#include <machine/asm.h>
#include <riscv/proc_reg.h>
#include "assym.s"

// the kernel is built without f and d, only these routines touch the user fp registers
	.option	arch, +d

	.text
	.align 2

	// void fp_save(riscv_float_saved_state_t *fp_ss)
	// the caller decides what sstatus.FS becomes afterwards
	.globl	EXT(fp_save)
LEXT(fp_save)
	li	t0, SSTATUS_FS_INITIAL
	csrs	sstatus, t0
	fsd	f0, FS_F0(a0)
	fsd	f1, FS_F1(a0)
	fsd	f2, FS_F2(a0)
	fsd	f3, FS_F3(a0)
	fsd	f4, FS_F4(a0)
	fsd	f5, FS_F5(a0)
	fsd	f6, FS_F6(a0)
	fsd	f7, FS_F7(a0)
	fsd	f8, FS_F8(a0)
	fsd	f9, FS_F9(a0)
	fsd	f10, FS_F10(a0)
	fsd	f11, FS_F11(a0)
	fsd	f12, FS_F12(a0)
	fsd	f13, FS_F13(a0)
	fsd	f14, FS_F14(a0)
	fsd	f15, FS_F15(a0)
	fsd	f16, FS_F16(a0)
	fsd	f17, FS_F17(a0)
	fsd	f18, FS_F18(a0)
	fsd	f19, FS_F19(a0)
	fsd	f20, FS_F20(a0)
	fsd	f21, FS_F21(a0)
	fsd	f22, FS_F22(a0)
	fsd	f23, FS_F23(a0)
	fsd	f24, FS_F24(a0)
	fsd	f25, FS_F25(a0)
	fsd	f26, FS_F26(a0)
	fsd	f27, FS_F27(a0)
	fsd	f28, FS_F28(a0)
	fsd	f29, FS_F29(a0)
	fsd	f30, FS_F30(a0)
	fsd	f31, FS_F31(a0)
	frcsr	t0
	sw	t0, FS_FCSR(a0)
	ret

	// void fp_load(riscv_float_saved_state_t *fp_ss)
	.globl	EXT(fp_load)
LEXT(fp_load)
	li	t0, SSTATUS_FS_INITIAL
	csrs	sstatus, t0
	fld	f0, FS_F0(a0)
	fld	f1, FS_F1(a0)
	fld	f2, FS_F2(a0)
	fld	f3, FS_F3(a0)
	fld	f4, FS_F4(a0)
	fld	f5, FS_F5(a0)
	fld	f6, FS_F6(a0)
	fld	f7, FS_F7(a0)
	fld	f8, FS_F8(a0)
	fld	f9, FS_F9(a0)
	fld	f10, FS_F10(a0)
	fld	f11, FS_F11(a0)
	fld	f12, FS_F12(a0)
	fld	f13, FS_F13(a0)
	fld	f14, FS_F14(a0)
	fld	f15, FS_F15(a0)
	fld	f16, FS_F16(a0)
	fld	f17, FS_F17(a0)
	fld	f18, FS_F18(a0)
	fld	f19, FS_F19(a0)
	fld	f20, FS_F20(a0)
	fld	f21, FS_F21(a0)
	fld	f22, FS_F22(a0)
	fld	f23, FS_F23(a0)
	fld	f24, FS_F24(a0)
	fld	f25, FS_F25(a0)
	fld	f26, FS_F26(a0)
	fld	f27, FS_F27(a0)
	fld	f28, FS_F28(a0)
	fld	f29, FS_F29(a0)
	fld	f30, FS_F30(a0)
	fld	f31, FS_F31(a0)
	lw	t0, FS_FCSR(a0)
	fscsr	t0
	ret

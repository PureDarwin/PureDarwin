#include <machine/asm.h>
#include <riscv/proc_reg.h>
#include "assym.s"
#include <riscv/machine_machdep.h>

// trap frames on a kernel stack stay 16 byte aligned
#define KFRAME_SIZE     ((SS_SIZE + 15) & ~15)

// every register but x0, x2 (sp), x4 (tp) and x5 (t0), which the entry paths save themselves
.macro SAVE_GPRS base
	sd	x1, SS_X1(\base)
	sd	x3, SS_X3(\base)
	sd	x6, SS_X6(\base)
	sd	x7, SS_X7(\base)
	sd	x8, SS_X8(\base)
	sd	x9, SS_X9(\base)
	sd	x10, SS_X10(\base)
	sd	x11, SS_X11(\base)
	sd	x12, SS_X12(\base)
	sd	x13, SS_X13(\base)
	sd	x14, SS_X14(\base)
	sd	x15, SS_X15(\base)
	sd	x16, SS_X16(\base)
	sd	x17, SS_X17(\base)
	sd	x18, SS_X18(\base)
	sd	x19, SS_X19(\base)
	sd	x20, SS_X20(\base)
	sd	x21, SS_X21(\base)
	sd	x22, SS_X22(\base)
	sd	x23, SS_X23(\base)
	sd	x24, SS_X24(\base)
	sd	x25, SS_X25(\base)
	sd	x26, SS_X26(\base)
	sd	x27, SS_X27(\base)
	sd	x28, SS_X28(\base)
	sd	x29, SS_X29(\base)
	sd	x30, SS_X30(\base)
	sd	x31, SS_X31(\base)
.endm

// gp stays live on kernel returns, a thread that blocked may resume on another cpu
.macro LOAD_GPRS base
	ld	x1, SS_X1(\base)
	ld	x6, SS_X6(\base)
	ld	x7, SS_X7(\base)
	ld	x8, SS_X8(\base)
	ld	x9, SS_X9(\base)
	ld	x10, SS_X10(\base)
	ld	x11, SS_X11(\base)
	ld	x12, SS_X12(\base)
	ld	x13, SS_X13(\base)
	ld	x14, SS_X14(\base)
	ld	x15, SS_X15(\base)
	ld	x16, SS_X16(\base)
	ld	x17, SS_X17(\base)
	ld	x18, SS_X18(\base)
	ld	x19, SS_X19(\base)
	ld	x20, SS_X20(\base)
	ld	x21, SS_X21(\base)
	ld	x22, SS_X22(\base)
	ld	x23, SS_X23(\base)
	ld	x24, SS_X24(\base)
	ld	x25, SS_X25(\base)
	ld	x26, SS_X26(\base)
	ld	x27, SS_X27(\base)
	ld	x28, SS_X28(\base)
	ld	x29, SS_X29(\base)
	ld	x30, SS_X30(\base)
	ld	x31, SS_X31(\base)
.endm

.macro SAVE_TRAP_CSRS base
	csrr	t1, sepc
	sd	t1, SS_PC(\base)
	csrr	t1, sstatus
	sd	t1, SS_SSTATUS(\base)
	csrr	t1, scause
	sd	t1, SS_SCAUSE(\base)
	csrr	t1, stval
	sd	t1, SS_STVAL(\base)
.endm

	.text
	.align 2
	.globl	EXT(riscv_trap_vector)
LEXT(riscv_trap_vector)
	// sscratch holds the thread while user code runs and 0 in the kernel
	csrrw	tp, sscratch, tp
	bnez	tp, Ltrap_from_user
	csrrw	tp, sscratch, tp

	// a kernel trap lands on the per cpu exception stack first, so an overflowed
	// thread stack is never touched, sscratch lends t0 until then
	csrw	sscratch, t0
	ld	t0, CPU_EXCEPSTACK_TOP(gp)
	sub	t0, t0, sp
	addi	t0, t0, -1
	srli	t0, t0, 14
	beqz	t0, Lnested_exception_stack
	ld	t0, CPU_EXCEPSTACK_TOP(gp)
	j	Lkernel_frame
Lnested_exception_stack:
	// already on the exception stack, push below the current frame
	mv	t0, sp
Lkernel_frame:
	addi	t0, t0, -KFRAME_SIZE
	sd	sp, SS_X2(t0)
	mv	sp, t0
	csrrw	t0, sscratch, zero
	sd	t0, SS_X5(sp)
	sd	tp, SS_X4(sp)
	SAVE_GPRS sp
	SAVE_TRAP_CSRS sp
	sd	zero, 0(sp)

	// the c side moves the frame to whichever stack should handle it and returns
	// the frame to resume from
	mv	a0, sp
	mv	s0, zero
	call	EXT(sleh_kernel)
	j	Lkernel_return_from_frame

	// resume the kernel from the trap frame in a0
	.globl	EXT(riscv_kernel_return)
LEXT(riscv_kernel_return)
Lkernel_return_from_frame:
	csrci	sstatus, SSTATUS_SIE
	mv	t0, a0
	ld	t1, SS_PC(t0)
	csrw	sepc, t1
	// the handler may have switched threads, keep the live fp state field
	ld	t1, SS_SSTATUS(t0)
	li	t2, SSTATUS_FS_MASK
	not	t2, t2
	and	t1, t1, t2
	csrr	t2, sstatus
	li	t3, SSTATUS_FS_MASK
	and	t2, t2, t3
	or	t1, t1, t2
	csrw	sstatus, t1
	LOAD_GPRS t0
	ld	tp, SS_X4(t0)
	ld	sp, SS_X2(t0)
	ld	t0, SS_X5(t0)
	sret

Ltrap_from_user:
	// tp is the thread, sscratch the user tp, t0 parks in the thread until the pcb is found
	sd	t0, TH_TRAP_SCRATCH(tp)
	ld	t0, TH_UPCB(tp)
	sd	sp, SS_X2(t0)
	SAVE_GPRS t0
	ld	t1, TH_TRAP_SCRATCH(tp)
	sd	t1, SS_X5(t0)
	csrrw	t1, sscratch, zero
	sd	t1, SS_X4(t0)
	SAVE_TRAP_CSRS t0

	ld	sp, TH_KSTACKPTR(tp)
	ld	gp, ACT_CPUDATAP(tp)
	mv	s0, zero
	mv	a0, t0
	call	EXT(sleh_user)
	// sleh_user leaves through thread_exception_return
	unimp

	// back to user space through the current thread's pcb, delivering pending asts first
	.globl	EXT(thread_exception_return)
LEXT(thread_exception_return)
	.globl	EXT(riscv_user_return)
LEXT(riscv_user_return)
	sb	zero, TH_USER_SYNC_TRAP(tp)
1:
	csrci	sstatus, SSTATUS_SIE
	ld	t0, ACT_CPUDATAP(tp)
	lw	t1, CPU_PENDING_AST(t0)
	beqz	t1, 2f
	call	EXT(ast_taken_user)
	j	1b
2:
	ld	t0, TH_UPCB(tp)
	ld	t1, SS_PC(t0)
	csrw	sepc, t1
	// keep the live fp state field, the pcb copy only records what the thread had at entry
	ld	t1, SS_SSTATUS(t0)
	li	t2, SSTATUS_FS_MASK
	not	t2, t2
	and	t1, t1, t2
	csrr	t2, sstatus
	li	t3, SSTATUS_FS_MASK
	and	t2, t2, t3
	or	t1, t1, t2
	li	t2, SSTATUS_SPP
	not	t2, t2
	and	t1, t1, t2
	ori	t1, t1, SSTATUS_SPIE
	csrw	sstatus, t1
	csrw	sscratch, tp
	// user gp is the cpu number and cluster id, gp still points at this cpu's data here
	lhu	t1, CPU_NUMBER(gp)
	lwu	t2, CPU_CLUSTER_ID(gp)
	slli	t2, t2, MACHDEP_GP_CLUSTERID_SHIFT
	li	t3, MACHDEP_GP_CLUSTERID_MASK
	and	t2, t2, t3
	li	t3, MACHDEP_GP_CPUNUM_MASK
	and	t1, t1, t3
	or	t1, t1, t2
	sd	t1, SS_X3(t0)
	LOAD_GPRS t0
	ld	gp, SS_X3(t0)
	ld	sp, SS_X2(t0)
	ld	tp, SS_X4(t0)
	ld	t0, SS_X5(t0)
	sret

	// run fn(frame) on a new stack after copying the trap frame there, then resume
	// the kernel from the copy, a0 = frame, a1 = new stack top, a2 = fn
	.globl	EXT(riscv_handle_on_stack)
LEXT(riscv_handle_on_stack)
	addi	a1, a1, -KFRAME_SIZE
	li	t0, 0
1:
	add	t1, a0, t0
	ld	t2, 0(t1)
	add	t1, a1, t0
	sd	t2, 0(t1)
	addi	t0, t0, 8
	li	t3, SS_SIZE
	blt	t0, t3, 1b
	mv	sp, a1
	mv	a0, a1
	mv	s1, a1
	jalr	a2
	mv	a0, s1
	j	Lkernel_return_from_frame

	// call fn(arg) on another stack and come back, a0 = arg, a1 = fn, a2 = stack top
	// the frame record keeps backtraces walking into the caller's stack
	.globl	EXT(riscv_call_on_stack)
LEXT(riscv_call_on_stack)
	addi	sp, sp, -16
	sd	ra, 8(sp)
	sd	s0, 0(sp)
	addi	s0, sp, 16
	andi	sp, a2, -16
	jalr	a1
	addi	sp, s0, -16
	ld	s0, 0(sp)
	ld	ra, 8(sp)
	addi	sp, sp, 16
	ret

	// resume after a fault in copyio or another recoverable kernel access, the
	// fault handler already pointed the frame's pc at the thread's recovery address
	.globl	EXT(riscv_fault_recover)
LEXT(riscv_fault_recover)
	j	Lkernel_return_from_frame

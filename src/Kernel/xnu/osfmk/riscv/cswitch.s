#include <machine/asm.h>
#include <riscv/proc_reg.h>
#include "assym.s"

// callee saved registers of a switched out thread live at its kstackptr
.macro SAVE_KERNEL_STATE base
	sd	s0, KSS_S0(\base)
	sd	s1, KSS_S1(\base)
	sd	s2, KSS_S2(\base)
	sd	s3, KSS_S3(\base)
	sd	s4, KSS_S4(\base)
	sd	s5, KSS_S5(\base)
	sd	s6, KSS_S6(\base)
	sd	s7, KSS_S7(\base)
	sd	s8, KSS_S8(\base)
	sd	s9, KSS_S9(\base)
	sd	s10, KSS_S10(\base)
	sd	s11, KSS_S11(\base)
	sd	ra, KSS_RA(\base)
	sd	sp, KSS_SP(\base)
.endm

.macro LOAD_KERNEL_STATE base
	ld	s0, KSS_S0(\base)
	ld	s1, KSS_S1(\base)
	ld	s2, KSS_S2(\base)
	ld	s3, KSS_S3(\base)
	ld	s4, KSS_S4(\base)
	ld	s5, KSS_S5(\base)
	ld	s6, KSS_S6(\base)
	ld	s7, KSS_S7(\base)
	ld	s8, KSS_S8(\base)
	ld	s9, KSS_S9(\base)
	ld	s10, KSS_S10(\base)
	ld	s11, KSS_S11(\base)
	ld	ra, KSS_RA(\base)
	ld	sp, KSS_SP(\base)
.endm

// tp is the thread, gp its cpu, and the cpu records it as active
.macro SET_THREAD_REGISTERS thread, tmp
	mv	tp, \thread
	ld	gp, ACT_CPUDATAP(\thread)
	sd	\thread, CPU_ACTIVE_THREAD(gp)
.endm

	.text
	.align 2

	// void machine_load_context(thread_t thread), first thread on a cpu
	.globl	EXT(machine_load_context)
LEXT(machine_load_context)
	SET_THREAD_REGISTERS a0, t0
	ld	t0, TH_KSTACKPTR(a0)
	LOAD_KERNEL_STATE t0
	mv	a0, zero
	ret

	// void Call_continuation(thread_continue_t cont, void *param, wait_result_t wresult, bool enable_interrupts)
	.globl	EXT(Call_continuation)
LEXT(Call_continuation)
	ld	sp, TH_KSTACKPTR(tp)
	mv	s0, zero
	mv	s1, a0
	mv	s2, a1
	mv	s3, a2
	beqz	a3, 1f
	li	a0, 1
	call	EXT(ml_set_interrupts_enabled)
1:
	mv	a0, s2
	mv	a1, s3
	jalr	s1
	mv	a0, tp
	tail	EXT(thread_terminate)

	// thread_t Switch_context(thread_t old, thread_continue_t cont, thread_t new)
	// returns old, in the new thread's context
	.globl	EXT(Switch_context)
LEXT(Switch_context)
	// a thread blocking on a continuation keeps nothing on its stack
	bnez	a1, Lswitch_threads
	ld	t0, TH_KSTACKPTR(a0)
	SAVE_KERNEL_STATE t0
Lswitch_threads:
	SET_THREAD_REGISTERS a2, t0
	ld	t0, TH_KSTACKPTR(a2)
	LOAD_KERNEL_STATE t0
	ret

	// thread_t Shutdown_context(void (*doshutdown)(processor_t), processor_t processor)
	.globl	EXT(Shutdown_context)
LEXT(Shutdown_context)
	ld	t0, TH_KSTACKPTR(tp)
	SAVE_KERNEL_STATE t0
	csrci	sstatus, SSTATUS_SIE
	ld	sp, CPU_ISTACKPTR(gp)
	tail	EXT(cpu_doshutdown)

	// thread_t Idle_context(void)
	.globl	EXT(Idle_context)
LEXT(Idle_context)
	ld	t0, TH_KSTACKPTR(tp)
	SAVE_KERNEL_STATE t0
	ld	sp, CPU_ISTACKPTR(gp)
	tail	EXT(cpu_idle)

	// void Idle_load_context(void), back into the idle thread after the hart woke
	.globl	EXT(Idle_load_context)
LEXT(Idle_load_context)
	ld	t0, TH_KSTACKPTR(tp)
	LOAD_KERNEL_STATE t0
	ret

	// void machine_set_current_thread(thread_t thread)
	.globl	EXT(machine_set_current_thread)
LEXT(machine_set_current_thread)
	SET_THREAD_REGISTERS a0, t0
	ret

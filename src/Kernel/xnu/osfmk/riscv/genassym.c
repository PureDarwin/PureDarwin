/*
 * Copyright (c) 2007 Apple Inc. All rights reserved.
 *
 * @APPLE_OSREFERENCE_LICENSE_HEADER_START@
 *
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. The rights granted to you under the License
 * may not be used to create, or enable the creation or redistribution of,
 * unlawful or unlicensed copies of an Apple operating system, or to
 * circumvent, violate, or enable the circumvention or violation of, any
 * terms of an Apple operating system software license agreement.
 *
 * Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this file.
 *
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 *
 * @APPLE_OSREFERENCE_LICENSE_HEADER_END@
 */
// Pass field offsets to assembly code.
#include <kern/ast.h>
#include <kern/thread.h>
#include <kern/task.h>
#include <kern/locks.h>
#include <kern/misc_protos.h>
#include <kern/syscall_sw.h>
#include <riscv/thread.h>
#include <mach/riscv/vm_param.h>
#include <riscv/misc_protos.h>
#include <riscv/pmap.h>
#include <riscv/trap.h>
#include <riscv/cpu_data_internal.h>
#include <riscv/cpu_internal.h>
#include <riscv/proc_reg.h>
#include <machine/static_if.h>
#include <pexpert/riscv/boot.h>

// the build compiles this to assembly and a sed script turns each DECLARE into a #define

#define DECLARE(SYM, VAL) \
	__asm("DEFINITION__define__" SYM ":\t .ascii \"%0\"" : : "i"  ((u_long)(VAL)))

#define DECLARE_SS_X(n) \
	DECLARE("SS_X" #n, offsetof(riscv_saved_state_t, x[n]))

#define DECLARE_FS_F(n) \
	DECLARE("FS_F" #n, offsetof(riscv_float_saved_state_t, f[n]))

int main(int     argc,
    char ** argv);

int
main(int     argc,
    char ** argv)
{
	DECLARE("AST_URGENT", AST_URGENT);

	DECLARE("TH_KSTACKPTR", offsetof(struct thread, machine.kstackptr));
	DECLARE("TH_KERNEL_STACK", offsetof(struct thread, kernel_stack));
	DECLARE("TH_THREAD_ID", offsetof(struct thread, thread_id));
	DECLARE("TH_TRAP_SCRATCH", offsetof(struct thread, machine.trap_scratch));
	DECLARE("TH_RISCV_MACHINE_FLAGS", offsetof(struct thread, machine.riscv_machine_flags));
	DECLARE("TH_CONTEXTDATA", offsetof(struct thread, machine.contextData));
	DECLARE("TH_UPCB", offsetof(struct thread, machine.upcb));
	DECLARE("TH_UFPCB", offsetof(struct thread, machine.ufpcb));
	DECLARE("TH_KPCB", offsetof(struct thread, machine.kpcb));
	DECLARE("TH_CTH_SELF", offsetof(struct thread, machine.cthread_self));
	DECLARE("TH_USER_SYNC_TRAP", offsetof(struct thread, machine.user_synchronous_trap));
	DECLARE("ACT_PREEMPT_CNT", offsetof(struct thread, machine.preemption_count));
	DECLARE("ACT_CPUDATAP", offsetof(struct thread, machine.CpuDatap));
	DECLARE("TH_CTH_DATA", offsetof(struct thread, machine.cthread_self));

	// the user trap frame
	DECLARE("SS_SIZE", sizeof(riscv_saved_state_t));
	DECLARE_SS_X(1); DECLARE_SS_X(2); DECLARE_SS_X(3); DECLARE_SS_X(4);
	DECLARE_SS_X(5); DECLARE_SS_X(6); DECLARE_SS_X(7); DECLARE_SS_X(8);
	DECLARE_SS_X(9); DECLARE_SS_X(10); DECLARE_SS_X(11); DECLARE_SS_X(12);
	DECLARE_SS_X(13); DECLARE_SS_X(14); DECLARE_SS_X(15); DECLARE_SS_X(16);
	DECLARE_SS_X(17); DECLARE_SS_X(18); DECLARE_SS_X(19); DECLARE_SS_X(20);
	DECLARE_SS_X(21); DECLARE_SS_X(22); DECLARE_SS_X(23); DECLARE_SS_X(24);
	DECLARE_SS_X(25); DECLARE_SS_X(26); DECLARE_SS_X(27); DECLARE_SS_X(28);
	DECLARE_SS_X(29); DECLARE_SS_X(30); DECLARE_SS_X(31);
	DECLARE("SS_PC", offsetof(riscv_saved_state_t, pc));
	DECLARE("SS_SSTATUS", offsetof(riscv_saved_state_t, sstatus));
	DECLARE("SS_SCAUSE", offsetof(riscv_saved_state_t, scause));
	DECLARE("SS_STVAL", offsetof(riscv_saved_state_t, stval));

	// the user fp state
	DECLARE("FS_SIZE", sizeof(riscv_float_saved_state_t));
	DECLARE_FS_F(0); DECLARE_FS_F(1); DECLARE_FS_F(2); DECLARE_FS_F(3);
	DECLARE_FS_F(4); DECLARE_FS_F(5); DECLARE_FS_F(6); DECLARE_FS_F(7);
	DECLARE_FS_F(8); DECLARE_FS_F(9); DECLARE_FS_F(10); DECLARE_FS_F(11);
	DECLARE_FS_F(12); DECLARE_FS_F(13); DECLARE_FS_F(14); DECLARE_FS_F(15);
	DECLARE_FS_F(16); DECLARE_FS_F(17); DECLARE_FS_F(18); DECLARE_FS_F(19);
	DECLARE_FS_F(20); DECLARE_FS_F(21); DECLARE_FS_F(22); DECLARE_FS_F(23);
	DECLARE_FS_F(24); DECLARE_FS_F(25); DECLARE_FS_F(26); DECLARE_FS_F(27);
	DECLARE_FS_F(28); DECLARE_FS_F(29); DECLARE_FS_F(30); DECLARE_FS_F(31);
	DECLARE("FS_FCSR", offsetof(riscv_float_saved_state_t, fcsr));

	// callee saved state kept at the top of the kernel stack while switched out
	DECLARE("KSS_S0", offsetof(struct riscv_kernel_saved_state, s[0]));
	DECLARE("KSS_S1", offsetof(struct riscv_kernel_saved_state, s[1]));
	DECLARE("KSS_S2", offsetof(struct riscv_kernel_saved_state, s[2]));
	DECLARE("KSS_S3", offsetof(struct riscv_kernel_saved_state, s[3]));
	DECLARE("KSS_S4", offsetof(struct riscv_kernel_saved_state, s[4]));
	DECLARE("KSS_S5", offsetof(struct riscv_kernel_saved_state, s[5]));
	DECLARE("KSS_S6", offsetof(struct riscv_kernel_saved_state, s[6]));
	DECLARE("KSS_S7", offsetof(struct riscv_kernel_saved_state, s[7]));
	DECLARE("KSS_S8", offsetof(struct riscv_kernel_saved_state, s[8]));
	DECLARE("KSS_S9", offsetof(struct riscv_kernel_saved_state, s[9]));
	DECLARE("KSS_S10", offsetof(struct riscv_kernel_saved_state, s[10]));
	DECLARE("KSS_S11", offsetof(struct riscv_kernel_saved_state, s[11]));
	DECLARE("KSS_RA", offsetof(struct riscv_kernel_saved_state, ra));
	DECLARE("KSS_SP", offsetof(struct riscv_kernel_saved_state, sp));
	DECLARE("TKS_MACHINE", offsetof(struct thread_kernel_state, machine));

	// per cpu data, reached through gp
	DECLARE("CPU_NUMBER", offsetof(cpu_data_t, cpu_number));
	DECLARE("CPU_CLUSTER_ID", offsetof(cpu_data_t, cpu_cluster_id));
	DECLARE("CPU_ISTACKPTR", offsetof(cpu_data_t, istackptr));
	DECLARE("CPU_INTSTACK_TOP", offsetof(cpu_data_t, intstack_top));
	DECLARE("CPU_EXCEPSTACKPTR", offsetof(cpu_data_t, excepstackptr));
	DECLARE("CPU_EXCEPSTACK_TOP", offsetof(cpu_data_t, excepstack_top));
	DECLARE("CPU_ACTIVE_THREAD", offsetof(cpu_data_t, cpu_active_thread));
	DECLARE("CPU_PENDING_AST", offsetof(cpu_data_t, cpu_pending_ast));
	DECLARE("CPU_INT_STATE", offsetof(cpu_data_t, cpu_int_state));
	DECLARE("CPU_PHYS_ID", offsetof(cpu_data_t, cpu_phys_id));
	DECLARE("CPU_DATA_SIZE", sizeof(cpu_data_t));
	DECLARE("CDE_VADDR", offsetof(cpu_data_entry_t, cpu_data_vaddr));
	DECLARE("CDE_PADDR", offsetof(cpu_data_entry_t, cpu_data_paddr));
	DECLARE("CDE_SIZE", sizeof(cpu_data_entry_t));

	DECLARE("PGBYTES", RISCV_PGBYTES);
	DECLARE("PGSHIFT", RISCV_PGSHIFT);
	DECLARE("KERNEL_STACK_SIZE", KERNEL_STACK_SIZE);
	DECLARE("INTSTACK_SIZE", INTSTACK_SIZE);
	DECLARE("EXCEPSTACK_SIZE", EXCEPSTACK_SIZE);
	DECLARE("MAX_CPUS", MAX_CPUS);

	// boot_args as the loader hands it over
	DECLARE("BA_VIRTBASE", offsetof(struct boot_args, virtBase));
	DECLARE("BA_PHYSBASE", offsetof(struct boot_args, physBase));
	DECLARE("BA_MEMSIZE", offsetof(struct boot_args, memSize));
	DECLARE("BA_TOP_OF_KERNEL_DATA", offsetof(struct boot_args, topOfKernelData));

	return 0;
}

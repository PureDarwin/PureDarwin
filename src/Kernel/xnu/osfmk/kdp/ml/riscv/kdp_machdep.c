/*
 * Copyright (c) 2000-2016 Apple Inc. All rights reserved.
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

#include <mach/mach_types.h>
#include <mach/exception_types.h>
#include <mach/machine.h>
#include <riscv/pmap.h>
#include <riscv/proc_reg.h>
#include <riscv/thread.h>
#include <riscv/trap_internal.h>
#include <riscv/cpu_data_internal.h>
#include <riscv/caches_internal.h>
#include <kdp/kdp_internal.h>
#include <kern/debug.h>
#include <IOKit/IOPlatformExpert.h>
#include <libkern/OSAtomic.h>
#include <vm/vm_map.h>
#include <riscv/misc_protos.h>

#define KDP_TEST_HARNESS 0
#if KDP_TEST_HARNESS
#define dprintf(x) kprintf x
#else
#define dprintf(x) do {} while (0)
#endif

void            halt_all_cpus(boolean_t);
void kdp_call(void);
int kdp_getc(void);
int machine_trace_thread64(thread_t thread,
    char * tracepos,
    char * tracebound,
    int nframes,
    uint32_t * thread_trace_flags);

void kdp_trap(unsigned int, struct riscv_saved_state * saved_state);

extern bool machine_trace_thread_validate_kva(vm_offset_t addr);

#if CONFIG_KDP_INTERACTIVE_DEBUGGING
void
kdp_exception(
	unsigned char * pkt, int * len, unsigned short * remote_port, unsigned int exception, unsigned int code, unsigned int subcode)
{
	struct {
		kdp_exception_t pkt;
		kdp_exc_info_t exc;
	} aligned_pkt;

	kdp_exception_t * rq = (kdp_exception_t *)&aligned_pkt;

	bcopy((char *)pkt, (char *)rq, sizeof(*rq));
	rq->hdr.request = KDP_EXCEPTION;
	rq->hdr.is_reply = 0;
	rq->hdr.seq = kdp.exception_seq;
	rq->hdr.key = 0;
	rq->hdr.len = sizeof(*rq) + sizeof(kdp_exc_info_t);

	rq->n_exc_info = 1;
	rq->exc_info[0].cpu = 0;
	rq->exc_info[0].exception = exception;
	rq->exc_info[0].code = code;
	rq->exc_info[0].subcode = subcode;

	rq->hdr.len += rq->n_exc_info * sizeof(kdp_exc_info_t);

	bcopy((char *)rq, (char *)pkt, rq->hdr.len);

	kdp.exception_ack_needed = TRUE;

	*remote_port = kdp.exception_port;
	*len = rq->hdr.len;
}

boolean_t
kdp_exception_ack(unsigned char * pkt, int len)
{
	kdp_exception_ack_t aligned_pkt;
	kdp_exception_ack_t * rq = (kdp_exception_ack_t *)&aligned_pkt;

	if ((unsigned)len < sizeof(*rq)) {
		return FALSE;
	}

	bcopy((char *)pkt, (char *)rq, sizeof(*rq));

	if (!rq->hdr.is_reply || rq->hdr.request != KDP_EXCEPTION) {
		return FALSE;
	}

	dprintf(("kdp_exception_ack seq %x %x\n", rq->hdr.seq, kdp.exception_seq));

	if (rq->hdr.seq == kdp.exception_seq) {
		kdp.exception_ack_needed = FALSE;
		kdp.exception_seq++;
	}
	return TRUE;
}

static void
kdp_getintegerstate(char * out_state)
{
	riscv_thread_state64_t thread_state64;
	riscv_saved_state_t *saved_state;

	saved_state = kdp.saved_state;

	bzero((char *) &thread_state64, sizeof(thread_state64));

	saved_state_to_thread_state64(saved_state, &thread_state64);

	bcopy((char *) &thread_state64, (char *) out_state, sizeof(thread_state64));
}

// the kdp register flavors are the mach thread state flavors, like arm64 does it
// lldb has no riscv kdp register layout yet, these follow riscv_thread_state64_t
kdp_error_t
kdp_machine_read_regs(__unused unsigned int cpu, unsigned int flavor, char * data, int * size)
{
	switch (flavor) {
	case RISCV_THREAD_STATE64:
		dprintf(("kdp_readregs THREAD_STATE64\n"));
		kdp_getintegerstate(data);
		*size = RISCV_THREAD_STATE64_COUNT * sizeof(int);
		return KDPERR_NO_ERROR;

	case RISCV_FLOAT_STATE64:
		// the kernel has no fp state of its own, report zeroes like arm's vfp state
		dprintf(("kdp_readregs THREAD_FPSTATE\n"));
		bzero((char *) data, sizeof(riscv_float_state64_t));
		*size = RISCV_FLOAT_STATE64_COUNT * sizeof(int);
		return KDPERR_NO_ERROR;

	case RISCV_EXCEPTION_STATE64: {
		riscv_exception_state64_t exc_state;
		riscv_saved_state_t *saved_state = kdp.saved_state;

		dprintf(("kdp_readregs EXCEPTION_STATE64\n"));
		exc_state.scause = saved_state->scause;
		exc_state.stval = saved_state->stval;
		exc_state.sepc = saved_state->pc;
		bcopy((char *) &exc_state, (char *) data, sizeof(exc_state));
		*size = RISCV_EXCEPTION_STATE64_COUNT * sizeof(int);
		return KDPERR_NO_ERROR;
	}

	default:
		dprintf(("kdp_readregs bad flavor %d\n"));
		return KDPERR_BADFLAVOR;
	}
}

static void
kdp_setintegerstate(char * state_in)
{
	riscv_thread_state64_t thread_state64;
	riscv_saved_state_t *saved_state;

	bcopy((char *) state_in, (char *) &thread_state64, sizeof(thread_state64));
	saved_state = kdp.saved_state;

	// gp holds the per cpu data pointer and tp the current thread, keep both
	thread_state64.x[RISCV_REG_GP] = get_saved_state_reg(saved_state, RISCV_REG_GP);
	thread_state64.x[RISCV_REG_TP] = get_saved_state_reg(saved_state, RISCV_REG_TP);

	// sstatus is outside the thread state, so the saved privilege mode stays as it was
	thread_state64_to_saved_state(&thread_state64, saved_state);
}

kdp_error_t
kdp_machine_write_regs(__unused unsigned int cpu, unsigned int flavor, char * data, __unused int * size)
{
	switch (flavor) {
	case RISCV_THREAD_STATE64:
		dprintf(("kdp_writeregs THREAD_STATE64\n"));
		kdp_setintegerstate(data);
		return KDPERR_NO_ERROR;

	case RISCV_FLOAT_STATE64:
		dprintf(("kdp_writeregs THREAD_FPSTATE\n"));
		return KDPERR_NO_ERROR;

	default:
		dprintf(("kdp_writeregs bad flavor %d\n"));
		return KDPERR_BADFLAVOR;
	}
}

void
kdp_machine_hostinfo(kdp_hostinfo_t * hostinfo)
{
	hostinfo->cpus_mask = 1;
	hostinfo->cpu_type = CPU_TYPE_RISCV64;
	hostinfo->cpu_subtype = CPU_SUBTYPE_RISCV_ALL;
}

__attribute__((noreturn))
void
kdp_panic(const char * fmt, ...)
{
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wformat-nonliteral"
#pragma clang diagnostic ignored "-Wformat"
	char kdp_fmt[256];
	va_list args;

	va_start(args, fmt);
	(void) snprintf(kdp_fmt, sizeof(kdp_fmt), "kdp panic: %s", fmt);
	vprintf(kdp_fmt, args);
	va_end(args);

	while (1) {
	}
	;
#pragma clang diagnostic pop
}

int
kdp_intr_disbl(void)
{
	return splhigh();
}

void
kdp_intr_enbl(int s)
{
	splx(s);
}

void
kdp_us_spin(int usec)
{
	delay(usec / 100);
}

void
kdp_call(void)
{
	Debugger("inline call to debugger(machine_startup)");
}

int
kdp_getc(void)
{
	return console_try_read_char();
}

// the uncompressed ebreak, sleh sends a bare one to DebuggerCall
void
kdp_machine_get_breakinsn(uint8_t * bytes, uint32_t * size)
{
	*(uint32_t *)bytes = RISCV_EBREAK;
	*size = sizeof(uint32_t);
}

// breakpoints are written through the physical aperture, fence.i on every hart makes them visible
void
kdp_sync_cache(void)
{
	invalidate_icache64(0, 0, FALSE);
}

int
kdp_machine_ioport_read(kdp_readioport_req_t * rq, caddr_t data, uint16_t lcpu)
{
#pragma unused(rq, data, lcpu)
	return 0;
}

int
kdp_machine_ioport_write(kdp_writeioport_req_t * rq, caddr_t data, uint16_t lcpu)
{
#pragma unused(rq, data, lcpu)
	return 0;
}

int
kdp_machine_msr64_read(kdp_readmsr64_req_t *rq, caddr_t data, uint16_t lcpu)
{
#pragma unused(rq, data, lcpu)
	return 0;
}

int
kdp_machine_msr64_write(kdp_writemsr64_req_t *rq, caddr_t data, uint16_t lcpu)
{
#pragma unused(rq, data, lcpu)
	return 0;
}
#endif /* CONFIG_KDP_INTERACTIVE_DEBUGGING */

void
kdp_trap(unsigned int exception, struct riscv_saved_state * saved_state)
{
	handle_debugger_trap(exception, 0, 0, saved_state);

	// step over the ebreak that got us here, kernel text may only be 2 byte aligned
	const uint16_t *pc = (const uint16_t *)get_saved_state_pc(saved_state);
	if (pc[0] == RISCV_C_EBREAK) {
		add_saved_state_pc(saved_state, 2);
	} else if (((uint32_t)pc[0] | ((uint32_t)pc[1] << 16)) == RISCV_EBREAK) {
		add_saved_state_pc(saved_state, 4);
	}
}

// the saved fp and ra sit just below the address in s0
#define RISCV64_FP_OFFSET (-16)
#define RISCV64_RA_OFFSET (-8)

int
machine_trace_thread64(thread_t thread,
    char * tracepos,
    char * tracebound,
    int nframes,
    uint32_t * thread_trace_flags)
{
	uint64_t * tracebuf = (uint64_t *)tracepos;
	vm_size_t framesize = sizeof(uint64_t);

	vm_offset_t stacklimit        = 0;
	vm_offset_t stacklimit_bottom = 0;
	int framecount                = 0;
	vm_offset_t pc                = 0;
	vm_offset_t fp                = 0;
	vm_offset_t sp                = 0;
	vm_offset_t prevfp            = 0;
	uint64_t prevlr               = 0;
	vm_offset_t kern_virt_addr    = 0;

	nframes = (tracebound > tracepos) ? MIN(nframes, (int)((tracebound - tracepos) / framesize)) : 0;
	if (!nframes) {
		return 0;
	}
	framecount = 0;

	struct riscv_saved_state *state = thread->machine.kpcb;
	if (state != NULL) {
		fp = get_saved_state_fp(state);
		prevlr = get_saved_state_lr(state);
		pc = get_saved_state_pc(state);
		sp = get_saved_state_sp(state);
	} else {
		// kstackptr may not always be there, so recompute it
		riscv_kernel_saved_state_t *kstate = &thread_get_kernel_state(thread)->machine.ss;

		fp = kstate->s[0];
		prevlr = kstate->ra;
		pc = 0;
		sp = kstate->sp;
	}

	stacklimit = VM_MAX_KERNEL_ADDRESS;
	stacklimit_bottom = VM_MIN_KERNEL_ADDRESS;

	if (!prevlr && !fp && !sp && !pc) {
		return 0;
	}

	prevlr = VM_KERNEL_UNSLIDE(prevlr);

	for (; framecount < nframes; framecount++) {
		*tracebuf++ = prevlr;

		/* Invalid frame */
		if (!fp) {
			break;
		}
		// sp is 16 byte aligned at every call, so the record below fp is 8 byte aligned
		if (fp & 0x0000007) {
			break;
		}
		/* Frame is out of range, maybe a user FP while doing kernel BT */
		if (fp > stacklimit) {
			break;
		}
		if (fp < stacklimit_bottom + 16) {
			break;
		}
		/* Stack grows downward */
		if (fp < prevfp) {
			bool switched_stacks = false;

			// As a special case, backtracing out of an interrupt handler can jump the stack
			// downward because of the early boot memory allocation pattern due to KASLR.
			int cpu;
			int max_cpu = ml_get_max_cpu_number();

			for (cpu = 0; cpu <= max_cpu; cpu++) {
				cpu_data_t      *target_cpu_datap;

				target_cpu_datap = (cpu_data_t *)CpuDataEntries[cpu].cpu_data_vaddr;
				if (target_cpu_datap == (cpu_data_t *)NULL) {
					continue;
				}

				// fp sits at the top of its frame, so the top of a stack counts as inside it
				if (prevfp > (target_cpu_datap->intstack_top - INTSTACK_SIZE) && prevfp <= target_cpu_datap->intstack_top) {
					switched_stacks = true;
					break;
				}
				if (prevfp > (target_cpu_datap->excepstack_top - EXCEPSTACK_SIZE) && prevfp <= target_cpu_datap->excepstack_top) {
					switched_stacks = true;
					break;
				}
			}

			// The stack can seem to grow upwards when this frame stitches two stacks together.
			// If both frames are in non-XNU stacks, assume we switched from one non-XNU stack to another.
			if ((ml_addr_in_non_xnu_stack(prevfp) != ml_addr_in_non_xnu_stack(fp)) ||
			    (ml_addr_in_non_xnu_stack(prevfp) && ml_addr_in_non_xnu_stack(fp))) {
				switched_stacks = true;
			}

			if (!switched_stacks) {
				/* Corrupt frame pointer? */
				break;
			}
		}

		/* Assume there's a saved return address, and read it */
		kern_virt_addr = fp + RISCV64_RA_OFFSET;
		bool ok = machine_trace_thread_validate_kva(kern_virt_addr);
		if (!ok) {
			if (thread_trace_flags != NULL) {
				*thread_trace_flags |= kThreadTruncatedBT;
			}

			break;
		}

		prevlr = *(uint64_t *)kern_virt_addr;
		prevlr = VM_KERNEL_UNSLIDE(prevlr);

		prevfp = fp;
		/* Next frame */
		kern_virt_addr = fp + RISCV64_FP_OFFSET;
		ok = machine_trace_thread_validate_kva(kern_virt_addr);
		if (!ok) {
			if (thread_trace_flags != NULL) {
				*thread_trace_flags |= kThreadTruncatedBT;
			}
			fp = 0;
			break;
		}

		fp = *(uint64_t *)kern_virt_addr;
	}
	return (int)(((char *)tracebuf) - tracepos);
}

// a bare 4 byte ebreak, sleh hands it to DebuggerCall and kdp_trap steps over it
void
kdp_ml_enter_debugger(void)
{
	__asm__ volatile (".option push\n.option norvc\nebreak\n.option pop");
}

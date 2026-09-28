#ifndef _RISCV_FP_INTERNAL_H_
#define _RISCV_FP_INTERNAL_H_

#include <kern/thread.h>
#include <riscv/proc_reg.h>
#include <riscv/thread.h>
#include <riscv/machine_routines.h>

// the kernel never uses fp, so the live sstatus.FS field and registers belong to
// the user state of the current thread until the next context switch

static inline uint64_t
fp_state_live(void)
{
	return csr_read(sstatus) & SSTATUS_FS_MASK;
}

// callers keep interrupts off, a switch in between could mark another thread's registers as ours
static inline void
fp_state_set_live(uint64_t fs)
{
	csr_clear(sstatus, SSTATUS_FS_MASK);
	csr_set(sstatus, fs & SSTATUS_FS_MASK);
}

// write registers the current thread changed back to its pcb
static inline void
fp_state_flush_current(thread_t thread)
{
	boolean_t istate = ml_set_interrupts_enabled(FALSE);

	if (fp_state_live() == SSTATUS_FS_DIRTY && thread->machine.ufpcb != NULL) {
		fp_save(thread->machine.ufpcb);
		fp_state_set_live(SSTATUS_FS_CLEAN);
	}
	ml_set_interrupts_enabled(istate);
}

// forget the live registers, the next fp instruction traps and loads the pcb
static inline void
fp_state_discard_current(void)
{
	csr_clear(sstatus, SSTATUS_FS_MASK);
}

#endif /* _RISCV_FP_INTERNAL_H_ */

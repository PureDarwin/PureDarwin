#ifndef _RISCV_SBI_H_
#define _RISCV_SBI_H_

// supervisor binary interface calls into the m-mode firmware, from the risc-v sbi spec v2.0
// a7 = extension, a6 = function, arguments in a0-a5, error back in a0 and value in a1

#include <stdint.h>
#include <stdbool.h>

#define SBI_EXT_BASE            0x10
#define SBI_EXT_TIME            0x54494D45
#define SBI_EXT_IPI             0x735049
#define SBI_EXT_RFENCE          0x52464E43
#define SBI_EXT_HSM             0x48534D
#define SBI_EXT_SRST            0x53525354

#define SBI_BASE_GET_SPEC_VERSION       0
#define SBI_BASE_PROBE_EXTENSION        3
#define SBI_BASE_GET_MVENDORID          4
#define SBI_BASE_GET_MARCHID            5
#define SBI_BASE_GET_MIMPID             6

#define SBI_RFENCE_REMOTE_FENCE_I               0
#define SBI_RFENCE_REMOTE_SFENCE_VMA            1
#define SBI_RFENCE_REMOTE_SFENCE_VMA_ASID       2

#define SBI_HSM_HART_START              0
#define SBI_HSM_HART_STOP               1
#define SBI_HSM_HART_GET_STATUS         2
#define SBI_HSM_HART_SUSPEND            3

#define SBI_HSM_STATE_STARTED           0
#define SBI_HSM_STATE_STOPPED           1
#define SBI_HSM_STATE_START_PENDING     2
#define SBI_HSM_STATE_STOP_PENDING      3

#define SBI_SRST_TYPE_SHUTDOWN          0
#define SBI_SRST_TYPE_COLD_REBOOT       1
#define SBI_SRST_TYPE_WARM_REBOOT       2

#define SBI_SUCCESS                     0
#define SBI_ERR_NOT_SUPPORTED           (-2)

// a hart mask covers 64 harts starting at hart_mask_base, a base of -1 means every hart
#define SBI_HART_MASK_BASE_ALL          ((unsigned long)-1)

struct sbiret {
	long error;
	long value;
};

static inline struct sbiret
sbi_ecall(unsigned long ext, unsigned long fid, unsigned long arg0,
    unsigned long arg1, unsigned long arg2, unsigned long arg3,
    unsigned long arg4, unsigned long arg5)
{
	register unsigned long a0 __asm__("a0") = arg0;
	register unsigned long a1 __asm__("a1") = arg1;
	register unsigned long a2 __asm__("a2") = arg2;
	register unsigned long a3 __asm__("a3") = arg3;
	register unsigned long a4 __asm__("a4") = arg4;
	register unsigned long a5 __asm__("a5") = arg5;
	register unsigned long a6 __asm__("a6") = fid;
	register unsigned long a7 __asm__("a7") = ext;

	__asm__ volatile ("ecall"
            : "+r"(a0), "+r"(a1)
            : "r"(a2), "r"(a3), "r"(a4), "r"(a5), "r"(a6), "r"(a7)
            : "memory");

	return (struct sbiret){ .error = (long)a0, .value = (long)a1 };
}

static inline bool
sbi_probe_extension(unsigned long ext)
{
	struct sbiret ret = sbi_ecall(SBI_EXT_BASE, SBI_BASE_PROBE_EXTENSION, ext, 0, 0, 0, 0, 0);
	return ret.error == SBI_SUCCESS && ret.value != 0;
}

// arms the supervisor timer interrupt at an absolute time value, clearing any pending one
static inline void
sbi_set_timer(uint64_t stime_value)
{
	(void)sbi_ecall(SBI_EXT_TIME, 0, stime_value, 0, 0, 0, 0, 0);
}

static inline long
sbi_send_ipi(unsigned long hart_mask, unsigned long hart_mask_base)
{
	return sbi_ecall(SBI_EXT_IPI, 0, hart_mask, hart_mask_base, 0, 0, 0, 0).error;
}

static inline long
sbi_remote_fence_i(unsigned long hart_mask, unsigned long hart_mask_base)
{
	return sbi_ecall(SBI_EXT_RFENCE, SBI_RFENCE_REMOTE_FENCE_I,
	           hart_mask, hart_mask_base, 0, 0, 0, 0).error;
}

// a size of -1 flushes the whole address space
static inline long
sbi_remote_sfence_vma(unsigned long hart_mask, unsigned long hart_mask_base,
    unsigned long start, unsigned long size)
{
	return sbi_ecall(SBI_EXT_RFENCE, SBI_RFENCE_REMOTE_SFENCE_VMA,
	           hart_mask, hart_mask_base, start, size, 0, 0).error;
}

static inline long
sbi_remote_sfence_vma_asid(unsigned long hart_mask, unsigned long hart_mask_base,
    unsigned long start, unsigned long size, unsigned long asid)
{
	return sbi_ecall(SBI_EXT_RFENCE, SBI_RFENCE_REMOTE_SFENCE_VMA_ASID,
	           hart_mask, hart_mask_base, start, size, asid, 0).error;
}

// the hart starts in s-mode with paging off at start_addr, a0 = hart id, a1 = opaque
static inline long
sbi_hart_start(unsigned long hartid, unsigned long start_addr, unsigned long opaque)
{
	return sbi_ecall(SBI_EXT_HSM, SBI_HSM_HART_START, hartid, start_addr, opaque, 0, 0, 0).error;
}

static inline long
sbi_hart_stop(void)
{
	return sbi_ecall(SBI_EXT_HSM, SBI_HSM_HART_STOP, 0, 0, 0, 0, 0, 0).error;
}

static inline long
sbi_hart_get_status(unsigned long hartid)
{
	struct sbiret ret = sbi_ecall(SBI_EXT_HSM, SBI_HSM_HART_GET_STATUS, hartid, 0, 0, 0, 0, 0);
	return ret.error != SBI_SUCCESS ? ret.error : ret.value;
}

static inline long
sbi_system_reset(unsigned long type, unsigned long reason)
{
	return sbi_ecall(SBI_EXT_SRST, 0, type, reason, 0, 0, 0, 0).error;
}

#endif /* _RISCV_SBI_H_ */

#ifndef _PEXPERT_RISCV_PE_SBI_H_
#define _PEXPERT_RISCV_PE_SBI_H_

#include <stdint.h>
#include <stdbool.h>

// supervisor binary interface calls into the m-mode firmware, written from the risc-v sbi spec
// a7 holds the extension id, a6 the function id, a0-a5 the arguments

#define PE_SBI_EXT_LEGACY_PUTCHAR       0x01UL
#define PE_SBI_EXT_LEGACY_GETCHAR       0x02UL
#define PE_SBI_EXT_BASE                 0x10UL
#define PE_SBI_EXT_SRST                 0x53525354UL    // "SRST"
#define PE_SBI_EXT_DBCN                 0x4442434EUL    // "DBCN"

#define PE_SBI_BASE_GET_SPEC_VERSION    0
#define PE_SBI_BASE_GET_IMPL_ID         1
#define PE_SBI_BASE_GET_IMPL_VERSION    2
#define PE_SBI_BASE_PROBE_EXTENSION     3

#define PE_SBI_DBCN_CONSOLE_WRITE       0
#define PE_SBI_DBCN_CONSOLE_READ        1
#define PE_SBI_DBCN_CONSOLE_WRITE_BYTE  2

#define PE_SBI_SRST_SYSTEM_RESET        0
#define PE_SBI_SRST_TYPE_SHUTDOWN       0
#define PE_SBI_SRST_TYPE_COLD_REBOOT    1
#define PE_SBI_SRST_TYPE_WARM_REBOOT    2
#define PE_SBI_SRST_REASON_NONE         0
#define PE_SBI_SRST_REASON_FAILURE      1

#define PE_SBI_SUCCESS                  0L
#define PE_SBI_ERR_FAILED               (-1L)
#define PE_SBI_ERR_NOT_SUPPORTED        (-2L)

struct pe_sbi_ret {
	long error;
	long value;
};

static inline struct pe_sbi_ret
pe_sbi_ecall(unsigned long eid, unsigned long fid,
    unsigned long arg0, unsigned long arg1, unsigned long arg2)
{
	register unsigned long a0 __asm__("a0") = arg0;
	register unsigned long a1 __asm__("a1") = arg1;
	register unsigned long a2 __asm__("a2") = arg2;
	register unsigned long a6 __asm__("a6") = fid;
	register unsigned long a7 __asm__("a7") = eid;

	__asm__ volatile ("ecall"
            : "+r"(a0), "+r"(a1)
            : "r"(a2), "r"(a6), "r"(a7)
            : "memory");

	return (struct pe_sbi_ret){ .error = (long)a0, .value = (long)a1 };
}

// legacy calls return their result in a0 only
static inline long
pe_sbi_legacy_ecall(unsigned long eid, unsigned long arg0)
{
	return pe_sbi_ecall(eid, 0, arg0, 0, 0).error;
}

static inline long
pe_sbi_get_spec_version(void)
{
	struct pe_sbi_ret ret = pe_sbi_ecall(PE_SBI_EXT_BASE, PE_SBI_BASE_GET_SPEC_VERSION, 0, 0, 0);

	// firmware predating v0.2 has no base extension
	return ret.error == PE_SBI_SUCCESS ? ret.value : 0;
}

static inline bool
pe_sbi_probe_extension(unsigned long eid)
{
	struct pe_sbi_ret ret = pe_sbi_ecall(PE_SBI_EXT_BASE, PE_SBI_BASE_PROBE_EXTENSION, eid, 0, 0);

	return ret.error == PE_SBI_SUCCESS && ret.value != 0;
}

static inline void
pe_sbi_legacy_putchar(char c)
{
	(void)pe_sbi_legacy_ecall(PE_SBI_EXT_LEGACY_PUTCHAR, (unsigned char)c);
}

// -1 when nothing is pending
static inline int
pe_sbi_legacy_getchar(void)
{
	return (int)pe_sbi_legacy_ecall(PE_SBI_EXT_LEGACY_GETCHAR, 0);
}

static inline long
pe_sbi_dbcn_write_byte(char c)
{
	return pe_sbi_ecall(PE_SBI_EXT_DBCN, PE_SBI_DBCN_CONSOLE_WRITE_BYTE, (unsigned char)c, 0, 0).error;
}

// the buffer is a physical address split into xlen halves, on rv64 the high half is 0
static inline struct pe_sbi_ret
pe_sbi_dbcn_read(unsigned long len, uint64_t buf_phys)
{
	return pe_sbi_ecall(PE_SBI_EXT_DBCN, PE_SBI_DBCN_CONSOLE_READ, len, (unsigned long)buf_phys, 0);
}

// only returns on failure
static inline long
pe_sbi_system_reset(uint32_t type, uint32_t reason)
{
	return pe_sbi_ecall(PE_SBI_EXT_SRST, PE_SBI_SRST_SYSTEM_RESET, type, reason, 0).error;
}

#endif /* _PEXPERT_RISCV_PE_SBI_H_ */

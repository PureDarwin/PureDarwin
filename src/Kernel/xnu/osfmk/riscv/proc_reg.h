#ifndef _RISCV_PROC_REG_H_
#define _RISCV_PROC_REG_H_

// supervisor csrs and sv39 page table formats

// constants usable from both c and assembly
#if defined(__ASSEMBLER__) || defined(ASSEMBLER)
#define RV_ULL(x)               x
#else
#define RV_ULL(x)               x ## ULL
#endif

#include <pexpert/riscv/board_config.h>

// one pset per cluster, the virt board has a single cluster of identical harts
#define MAX_PSETS               MAX_CPU_CLUSTERS

// thread groups back the clutch scheduler, as on arm
#define CONFIG_THREAD_GROUPS    1

#define RISCV_PGSHIFT           12
#define RISCV_PGBYTES           (RV_ULL(1) << RISCV_PGSHIFT)
#define RISCV_PGMASK            (RISCV_PGBYTES - 1)

// sstatus
#define SSTATUS_SIE             (RV_ULL(1) << 1)
#define SSTATUS_SPIE            (RV_ULL(1) << 5)
#define SSTATUS_UBE             (RV_ULL(1) << 6)
#define SSTATUS_SPP             (RV_ULL(1) << 8)
#define SSTATUS_VS_MASK         (RV_ULL(3) << 9)
#define SSTATUS_FS_SHIFT        13
#define SSTATUS_FS_MASK         (RV_ULL(3) << SSTATUS_FS_SHIFT)
#define SSTATUS_FS_OFF          (RV_ULL(0) << SSTATUS_FS_SHIFT)
#define SSTATUS_FS_INITIAL      (RV_ULL(1) << SSTATUS_FS_SHIFT)
#define SSTATUS_FS_CLEAN        (RV_ULL(2) << SSTATUS_FS_SHIFT)
#define SSTATUS_FS_DIRTY        (RV_ULL(3) << SSTATUS_FS_SHIFT)
#define SSTATUS_XS_MASK         (RV_ULL(3) << 15)
#define SSTATUS_SUM             (RV_ULL(1) << 18)
#define SSTATUS_MXR             (RV_ULL(1) << 19)
#define SSTATUS_UXL_SHIFT       32
#define SSTATUS_UXL_64          (RV_ULL(2) << SSTATUS_UXL_SHIFT)
#define SSTATUS_SD              (RV_ULL(1) << 63)

// sie and sip
#define SIE_SSIE                (RV_ULL(1) << 1)
#define SIE_STIE                (RV_ULL(1) << 5)
#define SIE_SEIE                (RV_ULL(1) << 9)

// scause
#define SCAUSE_INTERRUPT        (RV_ULL(1) << 63)
#define SCAUSE_CODE(c)          ((c) & ~SCAUSE_INTERRUPT)

// satp
#define SATP_MODE_SHIFT         60
#define SATP_MODE_BARE          (RV_ULL(0) << SATP_MODE_SHIFT)
#define SATP_MODE_SV39          (RV_ULL(8) << SATP_MODE_SHIFT)
#define SATP_ASID_SHIFT         44
#define SATP_ASID_MASK          (RV_ULL(0xffff) << SATP_ASID_SHIFT)
#define SATP_PPN_MASK           ((RV_ULL(1) << 44) - 1)
#define SATP_MAKE(asid, root_pa) \
	(SATP_MODE_SV39 | ((uint64_t)(asid) << SATP_ASID_SHIFT) | ((root_pa) >> RISCV_PGSHIFT))

// sv39 levels, 1 maps 1GB, 2 maps 2MB, 3 maps 4KB
#define RISCV_TT_ENTRIES        512
#define RISCV_TT_L1_SHIFT       30
#define RISCV_TT_L2_SHIFT       21
#define RISCV_TT_L3_SHIFT       12
#define RISCV_TT_L1_SIZE        (RV_ULL(1) << RISCV_TT_L1_SHIFT)
#define RISCV_TT_L2_SIZE        (RV_ULL(1) << RISCV_TT_L2_SHIFT)
#define RISCV_TT_L3_SIZE        (RV_ULL(1) << RISCV_TT_L3_SHIFT)
#define RISCV_TT_L1_INDEX(va)   (((va) >> RISCV_TT_L1_SHIFT) & 0x1ff)
#define RISCV_TT_L2_INDEX(va)   (((va) >> RISCV_TT_L2_SHIFT) & 0x1ff)
#define RISCV_TT_L3_INDEX(va)   (((va) >> RISCV_TT_L3_SHIFT) & 0x1ff)

// memory past topOfKernelData that start.s maps for the tables riscv_vm_init builds
#define RISCV_BOOT_ALLOC_MARGIN (16 << 20)
#define RISCV_VA_BITS           39

// pte bits, a leaf has any of r/w/x set, a table pointer has none
#define PTE_V                   (RV_ULL(1) << 0)
#define PTE_R                   (RV_ULL(1) << 1)
#define PTE_W                   (RV_ULL(1) << 2)
#define PTE_X                   (RV_ULL(1) << 3)
#define PTE_U                   (RV_ULL(1) << 4)
#define PTE_G                   (RV_ULL(1) << 5)
#define PTE_A                   (RV_ULL(1) << 6)
#define PTE_D                   (RV_ULL(1) << 7)
#define PTE_RSW_SHIFT           8
#define PTE_RSW_MASK            (RV_ULL(3) << PTE_RSW_SHIFT)
#define PTE_PPN_SHIFT           10
#define PTE_PPN_MASK            (((RV_ULL(1) << 44) - 1) << PTE_PPN_SHIFT)
#define PTE_LEAF_MASK           (PTE_R | PTE_W | PTE_X)

// the software bits, wired and "writable but kept read only to track modify"
#define PTE_SW_WIRED            (RV_ULL(1) << 8)
#define PTE_SW_WRITEABLE        (RV_ULL(1) << 9)

// svpbmt memory types
#define PTE_PBMT_SHIFT          61
#define PTE_PBMT_MASK           (RV_ULL(3) << PTE_PBMT_SHIFT)
#define PTE_PBMT_PMA            (RV_ULL(0) << PTE_PBMT_SHIFT)
#define PTE_PBMT_NC             (RV_ULL(1) << PTE_PBMT_SHIFT)
#define PTE_PBMT_IO             (RV_ULL(2) << PTE_PBMT_SHIFT)

// t-head c906 extended attributes when its maee bit is on
#define PTE_THEAD_SO            (RV_ULL(1) << 63)
#define PTE_THEAD_C             (RV_ULL(1) << 62)
#define PTE_THEAD_B             (RV_ULL(1) << 61)
#define PTE_THEAD_SH            (RV_ULL(1) << 60)
#define PTE_THEAD_MASK          (RV_ULL(0x1f) << 59)

#define PTE_TO_PA(pte)          ((((pte) & PTE_PPN_MASK) >> PTE_PPN_SHIFT) << RISCV_PGSHIFT)
#define PA_TO_PTE(pa)           ((((pa) >> RISCV_PGSHIFT) << PTE_PPN_SHIFT) & PTE_PPN_MASK)

#if !defined(__ASSEMBLER__) && !defined(ASSEMBLER)

#include <stdint.h>

#define csr_read(csr) ({ \
	uint64_t __v; \
	__asm__ volatile ("csrr %0, " #csr : "=r"(__v) :: "memory"); \
	__v; \
})

#define csr_write(csr, val) ({ \
	uint64_t __v = (uint64_t)(val); \
	__asm__ volatile ("csrw " #csr ", %0" :: "rK"(__v) : "memory"); \
})

#define csr_set(csr, val) ({ \
	uint64_t __v = (uint64_t)(val); \
	__asm__ volatile ("csrs " #csr ", %0" :: "rK"(__v) : "memory"); \
})

#define csr_clear(csr, val) ({ \
	uint64_t __v = (uint64_t)(val); \
	__asm__ volatile ("csrc " #csr ", %0" :: "rK"(__v) : "memory"); \
})

#define csr_read_clear(csr, val) ({ \
	uint64_t __v = (uint64_t)(val); \
	__asm__ volatile ("csrrc %0, " #csr ", %1" : "=r"(__v) : "rK"(__v) : "memory"); \
	__v; \
})

static inline void
sfence_vma_all(void)
{
	__asm__ volatile ("sfence.vma" ::: "memory");
}

static inline void
sfence_vma_asid(uint64_t asid)
{
	__asm__ volatile ("sfence.vma zero, %0" :: "r"(asid) : "memory");
}

static inline void
sfence_vma_va(uint64_t va)
{
	__asm__ volatile ("sfence.vma %0, zero" :: "r"(va) : "memory");
}

static inline void
sfence_vma_va_asid(uint64_t va, uint64_t asid)
{
	__asm__ volatile ("sfence.vma %0, %1" :: "r"(va), "r"(asid) : "memory");
}

#endif /* !__ASSEMBLER__ */

#endif /* _RISCV_PROC_REG_H_ */

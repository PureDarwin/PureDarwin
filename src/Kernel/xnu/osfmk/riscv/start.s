#include <machine/asm.h>
#include <riscv/proc_reg.h>
#include "assym.s"

// everything before riscv_init has run kernel_collection_slide must stay pc relative,
// so addresses come from lla and never from the got

// leaf gigapage, global, read write execute, accessed and dirty preset
#define BOOT_GIGAPAGE_BITS  (PTE_V | PTE_R | PTE_W | PTE_X | PTE_G | PTE_A | PTE_D)

	.text
	.align 2

	// entered in s-mode with paging off, a0 = physical boot_args, a1 = boot hart id
	.globl	EXT(_start)
LEXT(_start)
	csrw	sie, zero
	csrci	sstatus, SSTATUS_SIE
	csrw	sscratch, zero
	csrw	satp, zero
	sfence.vma
	li	t0, SSTATUS_FS_MASK
	csrc	sstatus, t0

	mv	s0, a0
	mv	s1, a1
	ld	s2, BA_PHYSBASE(s0)
	ld	s3, BA_VIRTBASE(s0)
	sub	s4, s3, s2
	// the loader keeps virtual minus physical 1GB aligned so gigapages can map it
	slli	t0, s4, 34
	bnez	t0, Lhang

	lla	s5, EXT(bootstrap_pagetable)
	li	t0, 512
	mv	t1, s5
1:
	sd	zero, 0(t1)
	addi	t1, t1, 8
	addi	t0, t0, -1
	bnez	t0, 1b

	// identity and kernel window gigapages from the gigabyte holding physBase to topOfKernelData
	// plus the early allocation margin riscv_vm_init builds its tables in
	srli	t2, s2, 30
	slli	t2, t2, 30
	ld	t3, BA_TOP_OF_KERNEL_DATA(s0)
	li	t0, RISCV_BOOT_ALLOC_MARGIN
	add	t3, t3, t0
2:
	bgeu	t2, t3, 3f
	srli	t4, t2, RISCV_PGSHIFT
	slli	t4, t4, PTE_PPN_SHIFT
	ori	t4, t4, BOOT_GIGAPAGE_BITS
	srli	t5, t2, RISCV_TT_L1_SHIFT
	andi	t5, t5, 511
	slli	t5, t5, 3
	add	t5, t5, s5
	sd	t4, 0(t5)
	add	t6, t2, s4
	srli	t6, t6, RISCV_TT_L1_SHIFT
	andi	t6, t6, 511
	slli	t6, t6, 3
	add	t6, t6, s5
	sd	t4, 0(t6)
	li	t5, 1
	slli	t5, t5, RISCV_TT_L1_SHIFT
	add	t2, t2, t5
	j	2b
3:
	srli	t0, s5, RISCV_PGSHIFT
	li	t1, 8
	slli	t1, t1, SATP_MODE_SHIFT
	or	t0, t0, t1
	sfence.vma
	csrw	satp, t0
	sfence.vma

	lla	t0, Lstart_high
	add	t0, t0, s4
	jr	t0

Lstart_high:
	// now at the kernel's virtual address, secondaries need the offset while still physical
	lla	t0, EXT(riscv_boot_va_offset)
	sd	s4, 0(t0)
	lla	t0, EXT(riscv_trap_vector)
	csrw	stvec, t0

	lla	gp, EXT(percpu_slot_cpu_data)
	lla	t0, EXT(intstack_top)
	sd	t0, CPU_INTSTACK_TOP(gp)
	sd	t0, CPU_ISTACKPTR(gp)
	lla	t0, EXT(excepstack_top)
	sd	t0, CPU_EXCEPSTACK_TOP(gp)
	sd	t0, CPU_EXCEPSTACKPTR(gp)
	sw	s1, CPU_PHYS_ID(gp)
	lla	sp, EXT(intstack_top)
	mv	tp, zero
	mv	s0, zero

	add	a0, a0, s4
	mv	a1, s1
	call	EXT(riscv_init)
Lhang:
	wfi
	j	Lhang

	// secondary harts start here through sbi hart_start, paging off,
	// a0 = hart id, a1 = physical cpu_data
	.globl	EXT(start_cpu)
LEXT(start_cpu)
	csrw	sie, zero
	csrci	sstatus, SSTATUS_SIE
	csrw	sscratch, zero
	li	t0, SSTATUS_FS_MASK
	csrc	sstatus, t0

	// the kernel tables have no identity map, the fetch fault after the satp write lands on stvec
	lla	t0, EXT(riscv_boot_va_offset)
	ld	s4, 0(t0)
	lla	t0, Lstart_cpu_high
	add	t0, t0, s4
	csrw	stvec, t0

	// a1 is the physical cpu_data, the hart finds it through the physmap
	lla	t0, EXT(riscv_physmap_offset)
	ld	t0, 0(t0)
	add	a1, a1, t0

	lla	t0, EXT(riscv_kernel_satp)
	ld	t0, 0(t0)
	sfence.vma
	csrw	satp, t0
	sfence.vma
	nop

	.align 2
Lstart_cpu_high:
	lla	t0, EXT(riscv_trap_vector)
	csrw	stvec, t0
	sfence.vma
	mv	gp, a1
	ld	sp, CPU_INTSTACK_TOP(gp)
	mv	tp, zero
	mv	s0, zero
	mv	a0, gp
	call	EXT(riscv_init_cpu)
	j	Lhang

	.section __DATA, __data
	.align 3
	.globl	EXT(riscv_boot_va_offset)
LEXT(riscv_boot_va_offset)
	.quad	0

	// set by riscv_vm_init for secondary harts, the kernel satp and physmap va minus pa
	.globl	EXT(riscv_kernel_satp)
LEXT(riscv_kernel_satp)
	.quad	0
	.globl	EXT(riscv_physmap_offset)
LEXT(riscv_physmap_offset)
	.quad	0

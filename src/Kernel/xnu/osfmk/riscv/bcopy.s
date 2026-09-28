#include <machine/asm.h>

	.text
	.align 2

	// void bcopy(const void *src, void *dst, size_t len)
	.globl	EXT(bcopy)
	.globl	EXT(ovbcopy)
LEXT(bcopy)
LEXT(ovbcopy)
	mv	t0, a0
	mv	a0, a1
	mv	a1, t0
	j	Lmemmove

	// void *memmove(void *dst, const void *src, size_t len)
	.globl	EXT(memmove)
LEXT(memmove)
Lmemmove:
	// copying forward is safe unless dst lands inside the source
	sub	t0, a0, a1
	bgeu	t0, a2, Lmemcpy
	// overlapping with dst above src, copy backward
	add	t1, a0, a2
	add	t2, a1, a2
	or	t3, t1, t2
	andi	t3, t3, 7
	bnez	t3, Lback_bytes
1:
	li	t3, 8
	bltu	a2, t3, Lback_bytes
	addi	t1, t1, -8
	addi	t2, t2, -8
	ld	t3, 0(t2)
	sd	t3, 0(t1)
	addi	a2, a2, -8
	j	1b
Lback_bytes:
	beqz	a2, 2f
	addi	t1, t1, -1
	addi	t2, t2, -1
	lbu	t3, 0(t2)
	sb	t3, 0(t1)
	addi	a2, a2, -1
	j	Lback_bytes
2:
	ret

	// void *memcpy(void *dst, const void *src, size_t len)
	.globl	EXT(memcpy)
LEXT(memcpy)
Lmemcpy:
	mv	t1, a0
	// word copies need dst and src at the same offset within a doubleword
	xor	t3, a0, a1
	andi	t3, t3, 7
	bnez	t3, Lfwd_bytes
	// bring both up to 8 byte alignment
3:
	andi	t3, t1, 7
	beqz	t3, 4f
	beqz	a2, Ldone
	lbu	t3, 0(a1)
	sb	t3, 0(t1)
	addi	t1, t1, 1
	addi	a1, a1, 1
	addi	a2, a2, -1
	j	3b
4:
	// 32 bytes a round while there is that much left
	li	t4, 32
5:
	bltu	a2, t4, 6f
	ld	t2, 0(a1)
	ld	t3, 8(a1)
	ld	t5, 16(a1)
	ld	t6, 24(a1)
	sd	t2, 0(t1)
	sd	t3, 8(t1)
	sd	t5, 16(t1)
	sd	t6, 24(t1)
	addi	t1, t1, 32
	addi	a1, a1, 32
	addi	a2, a2, -32
	j	5b
6:
	li	t4, 8
7:
	bltu	a2, t4, Lfwd_bytes
	ld	t2, 0(a1)
	sd	t2, 0(t1)
	addi	t1, t1, 8
	addi	a1, a1, 8
	addi	a2, a2, -8
	j	7b
Lfwd_bytes:
	beqz	a2, Ldone
	lbu	t3, 0(a1)
	sb	t3, 0(t1)
	addi	t1, t1, 1
	addi	a1, a1, 1
	addi	a2, a2, -1
	j	Lfwd_bytes
Ldone:
	ret

	// void bzero(void *s, size_t len)
	.globl	EXT(bzero)
	.globl	EXT(__bzero)
LEXT(bzero)
LEXT(__bzero)
	mv	a2, a1
	li	a1, 0
	j	Lmemset

	// void *memset(void *s, int c, size_t len), secure_memset is never elided because it lives here
	.globl	EXT(memset)
	.globl	EXT(secure_memset)
LEXT(memset)
LEXT(secure_memset)
Lmemset:
	mv	t1, a0
	andi	a1, a1, 0xff
	// the byte in every lane of a doubleword
	slli	t2, a1, 8
	or	a1, a1, t2
	slli	t2, a1, 16
	or	a1, a1, t2
	slli	t2, a1, 32
	or	a1, a1, t2
8:
	andi	t3, t1, 7
	beqz	t3, 9f
	beqz	a2, 12f
	sb	a1, 0(t1)
	addi	t1, t1, 1
	addi	a2, a2, -1
	j	8b
9:
	li	t4, 32
10:
	bltu	a2, t4, 11f
	sd	a1, 0(t1)
	sd	a1, 8(t1)
	sd	a1, 16(t1)
	sd	a1, 24(t1)
	addi	t1, t1, 32
	addi	a2, a2, -32
	j	10b
11:
	li	t4, 8
	bltu	a2, t4, 13f
	sd	a1, 0(t1)
	addi	t1, t1, 8
	addi	a2, a2, -8
	j	11b
13:
	beqz	a2, 12f
	sb	a1, 0(t1)
	addi	t1, t1, 1
	addi	a2, a2, -1
	j	13b
12:
	ret

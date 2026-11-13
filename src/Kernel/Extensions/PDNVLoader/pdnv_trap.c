// pdnv_trap.c - named traps for the imports the port does not define yet: a call says which one, and with what
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause
#include <stdint.h>
#include <string.h>
#include <IOKit/IOLib.h>

#include "pdnv_port.h"

#define PDNV_TRAPS 512

// trampoline i loads i into r11, which carries no argument, and joins the common path. Each is 16 bytes
__asm__(
	".text\n"
	".p2align 4\n"
	".globl _pdnv_trampolines\n"
	"_pdnv_trampolines:\n"
	".set pdnv_i, 0\n"
	".rept 512\n"
	"	movl $pdnv_i, %r11d\n"
	"	jmp _pdnv_trap_common\n"
	"	.p2align 4\n"
	"	.set pdnv_i, pdnv_i + 1\n"
	".endr\n"
	// the six argument registers to the stack, the C side sees them as an array. The return address left
	// rsp at 8 mod 16, six pushes and one more slot make the call aligned
	".p2align 4\n"
	"_pdnv_trap_common:\n"
	"	pushq %r9\n"
	"	pushq %r8\n"
	"	pushq %rcx\n"
	"	pushq %rdx\n"
	"	pushq %rsi\n"
	"	pushq %rdi\n"
	"	movl %r11d, %edi\n"
	"	movq %rsp, %rsi\n"
	"	subq $8, %rsp\n"
	"	call _pdnv_trap\n"
	"	addq $56, %rsp\n"
	"	ret\n"
);

extern char pdnv_trampolines[];

// copies: the names point into the object's string table, which is freed once it is linked
static char sNames[PDNV_TRAPS][64];
static uint8_t sSeen[PDNV_TRAPS];
static unsigned sCount;
static int sTrace;

void
pdnv_trap_set_trace(int on)
{
	sTrace = on;
}

void *
pdnv_trap_slot(const char *name)
{
	if (sCount == PDNV_TRAPS)
		return NULL;

	strlcpy(sNames[sCount], name, sizeof(sNames[0]));
	return pdnv_trampolines + 16 * sCount++;
}

// an unported import was called: by default stop there, with pdnvtrace=1 note it once and return 0
uint64_t pdnv_trap(uint32_t index, const uint64_t *args);

uint64_t
pdnv_trap(uint32_t index, const uint64_t *args)
{
	const char *name = index < sCount ? sNames[index] : "?";

	if (!sTrace)
		panic("PDNVLoader: unported import %s(0x%llx, 0x%llx, 0x%llx, 0x%llx, 0x%llx, 0x%llx)", name,
		    args[0], args[1], args[2], args[3], args[4], args[5]);

	if (index < PDNV_TRAPS && !sSeen[index]) {
		sSeen[index] = 1;
		IOLog("PDNVLoader: unported import %s(0x%llx, 0x%llx, 0x%llx, ...) returns 0\n", name, args[0],
		    args[1], args[2]);
	}
	return 0;
}

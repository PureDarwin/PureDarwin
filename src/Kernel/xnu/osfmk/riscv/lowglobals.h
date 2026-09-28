/*
 * Copyright (c) 2017 Apple Inc. All rights reserved.
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
// Header files for the Low Memory Globals (lg)
#ifndef _LOW_MEMORY_GLOBALS_H_
#define _LOW_MEMORY_GLOBALS_H_

#include <mach/mach_types.h>
#include <mach/vm_types.h>
#include <mach/machine/vm_types.h>
#include <mach/vm_prot.h>

#ifndef __riscv
#error  Wrong architecture - this file is meant for riscv
#endif

#define LOWGLO_LAYOUT_MAGIC             0xC0DEC0DE

// This structure is bound to lowmem_vectors.c. Make sure changes here are
// reflected there as well.

#pragma pack(8) /* Make sure the structure stays as we defined it */
typedef struct lowglo {
	unsigned char lgVerCode[8];            /* System verification code */
	uint64_t      lgZero;                  /* Constant 0 */
	uint64_t      lgStext;                 /* Start of kernel text */
	uint64_t      lgVersion;               /* Pointer to kernel version string */
	uint64_t      lgOSVersion;             /* Pointer to OS version string */
	uint64_t      lgKmodptr;               /* Pointer to kmod, debugging aid */
	uint64_t      lgTransOff;              /* Pointer to kdp_trans_off, debugging aid */
	uint64_t      lgRebootFlag;            /* Pointer to debugger reboot trigger */
	uint64_t      lgManualPktAddr;         /* Pointer to manual packet structure */
	uint64_t      lgAltDebugger;           /* Pointer to reserved space for alternate kernel debugger */
	uint64_t      lgPmapMemQ;              /* Pointer to PMAP memory queue */
	uint64_t      lgPmapMemPageOffset;     /* Offset of physical page member in vm_page_t or vm_page_with_ppnum_t */
	uint64_t      lgPmapMemChainOffset;    /* Offset of listq in vm_page_t or vm_page_with_ppnum_t */
	uint64_t      lgStaticAddr;            /* Static allocation address */
	uint64_t      lgStaticSize;            /* Static allocation size */
	uint64_t      lgLayoutMajorVersion;    /* Lowglo major layout version */
	uint64_t      lgLayoutMagic;           /* Magic value evaluated to determine if lgLayoutVersion is valid */
	uint64_t      lgPmapMemStartAddr;      /* Pointer to start of vm_page_t array */
	uint64_t      lgPmapMemEndAddr;        /* Pointer to end of vm_page_t array */
	uint64_t      lgPmapMemPagesize;       /* size of vm_page_t */
	uint64_t      lgPmapMemFromArrayMask;  /* Mask to indicate page is from vm_page_t array */
	uint64_t      lgPmapMemFirstppnum;     /* physical page number of the first vm_page_t in the array */
	uint64_t      lgPmapMemPackedShift;    /* alignment of packed pointer */
	uint64_t      lgPmapMemPackedBaseAddr; /* base address of that packed pointers are relative to */
	uint64_t      lgLayoutMinorVersion;    /* Lowglo minor layout version */
	uint64_t      lgPageShift;             /* number of shifts from page number to size */
	uint64_t      lgVmFirstPhys;           /* First physical address of kernel-managed DRAM (inclusive) */
	uint64_t      lgVmLastPhys;            /* Last physical address of kernel-managed DRAM (exclusive) */
	uint64_t      lgPhysMapBase;           /* First virtual address of the Physical Aperture (inclusive) */
	uint64_t      lgPhysMapEnd;            /* Last virtual address of the Physical Aperture (exclusive) */
	uint64_t      lgPmapIoRangePtr;        /* Pointer to an array of pmap_io_range_t objects obtained from the device tree. */
	uint64_t      lgNumPmapIoRanges;       /* Number of pmap_io_range regions in the array represented by lgPmapIoRangePtr. */
	uint64_t      lgCompressorBufferAddr;  /* Pointer to compressor buffer */
	uint64_t      lgCompressorSizeAddr;    /* Pointer to size of compressor buffer */
} lowglo;
#pragma pack()

extern lowglo lowGlo;

void patch_low_glo(void);
void patch_low_glo_static_region(uint64_t address, uint64_t size);
void patch_low_glo_vm_page_info(void *, void *, uint32_t);

#endif /* _LOW_MEMORY_GLOBALS_H_ */

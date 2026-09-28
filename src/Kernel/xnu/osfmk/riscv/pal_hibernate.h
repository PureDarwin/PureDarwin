/*
 * Copyright (c) 2020 Apple Inc. All rights reserved.
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
#ifndef _RISCV_PAL_HIBERNATE_H
#define _RISCV_PAL_HIBERNATE_H

#include <IOKit/IOHibernatePrivate.h>

__BEGIN_DECLS

typedef enum {
	DEST_COPY_AREA = 1,
	COPY_PAGE_AREA,
	BITMAP_AREA,
	IMAGE_AREA,
	IMAGE2_AREA,
	SCRATCH_AREA,
	WKDM_AREA,
} pal_hib_map_type_t;

struct pal_hib_ctx {
};

typedef struct {
	uint64_t hibUartRegPhysBase;
	uint64_t hibUartRegVirtBase;
	uint64_t kernelSlide;
} pal_hib_globals_t;
extern pal_hib_globals_t gHibernateGlobals;

void pal_hib_get_stack_pages(vm_offset_t *first_page, vm_offset_t *page_count);

void pal_hib_resume_tramp(uint32_t headerPpnum);

typedef struct{
	uint64_t satp;
	uint64_t memSlide;
} pal_hib_tramp_result_t;

__END_DECLS

#endif /* _RISCV_PAL_HIBERNATE_H */

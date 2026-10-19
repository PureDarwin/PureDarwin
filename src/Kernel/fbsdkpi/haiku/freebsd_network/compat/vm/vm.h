/*
 * Copyright 2022, Haiku, Inc. All rights reserved.
 * Distributed under the terms of the MIT License.
 */
#ifndef _FBSD_COMPAT_VM_VM_H_
#define _FBSD_COMPAT_VM_VM_H_


#include <KernelExport.h>


typedef phys_addr_t vm_paddr_t;
typedef addr_t vm_offset_t;

typedef void* pmap_t;


vm_paddr_t pmap_kextract(vm_offset_t virtualAddress);

#define vtophys(virtualAddress) pmap_kextract((vm_offset_t)(virtualAddress))
#define vmspace_pmap(...)	NULL
#define pmap_extract(...)	NULL


#endif	/* _FBSD_COMPAT_VM_VM_H_ */

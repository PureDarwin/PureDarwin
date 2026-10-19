// SPDX-License-Identifier: MIT
// haiku's area creation with physical restrictions, for contigmalloc
#ifndef _PD_HAIKU_KERNEL_VM_VM_H
#define _PD_HAIKU_KERNEL_VM_VM_H
#include <OS.h>
#include <KernelExport.h>
#include <kernel.h>
typedef struct virtual_address_restrictions {
	void	*address;
	uint32	address_specification;
	size_t	alignment;
} virtual_address_restrictions;
typedef struct physical_address_restrictions {
	phys_addr_t	low_address;
	phys_addr_t	high_address;
	phys_size_t	alignment;
	phys_size_t	boundary;
} physical_address_restrictions;
#define CREATE_AREA_DONT_WAIT	0x01
#define CREATE_AREA_DONT_CLEAR	0x04
#ifdef __cplusplus
extern "C" {
#endif
area_id create_area_etc(team_id team, const char *name, size_t size,
	uint32 lock, uint32 protection, uint32 flags, uint32 guardSize,
	const virtual_address_restrictions *virtualRestrictions,
	const physical_address_restrictions *physicalRestrictions, void **_address);
#ifdef __cplusplus
}
#endif
#endif

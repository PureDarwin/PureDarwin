/*
 * Copyright 2012, Haiku, Inc. All Rights Reserved.
 * Distributed under the terms of the MIT License.
 */
#ifndef _BIOS_MODULE_H_
#define _BIOS_MODULE_H_


#include <OS.h>
#include <module.h>


/*!	BIOS call interface.

	This module provides a mechanism to call PC BIOS interrupts (e.g. to use
	the VESA BIOS).

	Basic usage is as follows:
	 - Call bios_module_info::prepare(). This sets up memory mappings and
	   obtains exclusive access to the BIOS (only 1 thread is able to use the
	   BIOS at a time).
	 - Allocate memory for data that will be passed to BIOS interrupts using
	   bios_module_info::allocate_mem(). This returns a virtual address, to
	   get the physical address to pass to the BIOS use
	   bios_module_info::physical_address().
	 - Call the BIOS with bios_module_info::interrupt().
	 - Get the virtual location of any physical addresses returned using
	   bios_module_info::virtual_address().
	 - Release the BIOS and free created memory mappings with
	   bios_module_info::finish().

*/


// Cookie for the BIOS module functions.
typedef struct BIOSState bios_state;


// Registers to pass to a BIOS interrupt.
struct bios_regs {
	uint32	eax;
	uint32	ebx;
	uint32	ecx;
	uint32	edx;
	uint32	edi;
	uint32	esi;
	uint32	ebp;
	uint32	eflags;
	uint32	ds;
	uint32	es;
	uint32	fs;
	uint32	gs;
};


struct bios_module_info {
	module_info	info;

	status_t	(*prepare)(bios_state** _state);
	status_t	(*interrupt)(bios_state* state, uint8 vector, bios_regs* regs);
	void		(*finish)(bios_state* state);

	// Memory management methods.
	void*		(*allocate_mem)(bios_state* state, size_t size);
	uint32		(*physical_address)(bios_state* state, void* virtualAddress);
	void*		(*virtual_address)(bios_state* state, uint32 physicalAddress);
};


#define B_BIOS_MODULE_NAME "generic/bios/v1"


#endif	// _BIOS_MODULE_H_

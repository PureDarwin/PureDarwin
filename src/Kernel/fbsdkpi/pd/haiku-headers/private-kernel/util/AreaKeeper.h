/*
 * Copyright 2006-2008, Haiku, Inc. All Rights Reserved.
 * Distributed under the terms of the MIT License.
 *
 * Authors:
 *		Axel Dörfler, axeld@pinc-software.de
 */
#ifndef AREA_KEEPER_H
#define AREA_KEEPER_H


#include <KernelExport.h>
#include <OS.h>

#include <util/kernel_cpp.h>


class AreaKeeper {
	public:
		AreaKeeper();
		~AreaKeeper();

		area_id Create(const char *name, void **_virtualAddress, uint32 spec,
			size_t size, uint32 lock, uint32 protection);
		area_id Map(const char *name, phys_addr_t physicalAddress,
			size_t numBytes, uint32 spec, uint32 protection,
			void **_virtualAddress);

		status_t InitCheck() { return fArea < B_OK ? (status_t)fArea : B_OK; }
		void Detach();

	private:
		area_id	fArea;
};


AreaKeeper::AreaKeeper()
	:
	fArea(-1)
{
}


AreaKeeper::~AreaKeeper()
{
	if (fArea >= B_OK)
		delete_area(fArea);
}


area_id
AreaKeeper::Create(const char *name, void **_virtualAddress, uint32 spec,
	size_t size, uint32 lock, uint32 protection)
{
	fArea = create_area(name, _virtualAddress, spec, size, lock, protection);
	return fArea;
}


area_id
AreaKeeper::Map(const char *name, phys_addr_t physicalAddress, size_t numBytes,
	uint32 spec, uint32 protection, void **_virtualAddress)
{
	fArea = map_physical_memory(name, physicalAddress, numBytes, spec,
		protection, _virtualAddress);
	return fArea;
}


void
AreaKeeper::Detach()
{
	fArea = -1;
}

#endif	// AREA_KEEPER_H

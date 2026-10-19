/*
 * Copyright 2011, Ingo Weinhold, ingo_weinhold@gmx.de.
 * Distributed under the terms of the MIT License.
 */
#ifndef _KERNEL_UTIL_KERNEL_REFERENCEABLE_H
#define _KERNEL_UTIL_KERNEL_REFERENCEABLE_H


#include <Referenceable.h>

#include <heap.h>


namespace BKernel {


struct KernelReferenceable : BReferenceable, DeferredDeletable {
protected:
	virtual	void				LastReferenceReleased();
};


}	// namespace BKernel


using BKernel::KernelReferenceable;


#endif	/* _KERNEL_UTIL_KERNEL_REFERENCEABLE_H */

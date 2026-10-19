/*
 * Copyright 2022, Haiku, Inc. All rights reserved.
 * Distributed under the terms of the MIT license.
 */
#ifndef _UTIL_IOVEC_SUPPORT_H
#define _UTIL_IOVEC_SUPPORT_H


#include <KernelExport.h>


typedef struct generic_io_vec {
	generic_addr_t	base;
	generic_size_t	length;
} generic_io_vec;


#ifdef _KERNEL_VM_VM_H

static inline status_t
generic_memcpy(generic_addr_t dest, bool destPhysical, generic_addr_t src, bool srcPhysical,
	generic_size_t size, bool user = false)
{
	if (!srcPhysical && !destPhysical) {
		if (user)
			return user_memcpy((void*)dest, (void*)src, size);
		memcpy((void*)dest, (void*)src, size);
		return B_OK;
	} else if (destPhysical && !srcPhysical) {
		return vm_memcpy_to_physical(dest, (const void*)src, size, user);
	} else if (!destPhysical && srcPhysical) {
		return vm_memcpy_from_physical((void*)dest, src, size, user);
	}

	panic("generic_memcpy: physical -> physical not supported!");
	return B_NOT_SUPPORTED;
}

#endif


#ifdef IS_USER_ADDRESS

/*!
 * Copies an array of `iovec`s from userland.
 * Callers must verify vecCount <= IOV_MAX and supply their own vecs buffer.
 */
static inline status_t
get_iovecs_from_user(const iovec* userVecs, size_t vecCount, iovec* vecs,
	bool permitNull = false)
{
	if (vecCount == 0)
		return B_BAD_VALUE;

	if (!IS_USER_ADDRESS(userVecs))
		return B_BAD_ADDRESS;

	if (user_memcpy(vecs, userVecs, sizeof(iovec) * vecCount) != B_OK)
		return B_BAD_ADDRESS;

	size_t total = 0;
	for (size_t i = 0; i < vecCount; i++) {
		if (permitNull && vecs[i].iov_base == NULL)
			continue;
		if (!is_user_address_range(vecs[i].iov_base, vecs[i].iov_len)) {
			return B_BAD_ADDRESS;
		}
		if (vecs[i].iov_len > SSIZE_MAX || total > (SSIZE_MAX - vecs[i].iov_len)) {
			return B_BAD_VALUE;
		}
		total += vecs[i].iov_len;
	}

	return B_OK;
}

#endif


#endif	// _UTIL_IOVEC_SUPPORT_H

/*
 * Copyright 2013, Axel Dörfler, axeld@pinc-software.de.
 * Distributed under the terms of the MIT license.
 */
#ifndef _FS_TRIM_SUPPORT_H
#define _FS_TRIM_SUPPORT_H


#include <AutoDeleter.h>
#include <KernelExport.h>
#include <SupportDefs.h>

#include <kernel.h>
#include <syscall_restart.h>


static inline status_t
get_trim_data_from_user(void* buffer, size_t size, MemoryDeleter& deleter,
	fs_trim_data*& _trimData)
{
	if (!is_called_via_syscall() && !IS_USER_ADDRESS(buffer)) {
		// Called from kernel
		_trimData = (fs_trim_data*)buffer;
		return B_OK;
	}

	// Called from userland
	if (!IS_USER_ADDRESS(buffer))
		return B_BAD_ADDRESS;

	uint32 count;
	if (user_memcpy(&count, buffer, sizeof(count)) != B_OK)
		return B_BAD_ADDRESS;

	size_t bytes = (count - 1) * sizeof(uint64) * 2 + sizeof(fs_trim_data);
	if (bytes > size)
		return B_BAD_VALUE;

	void* trimBuffer = malloc(bytes);
	if (trimBuffer == NULL)
		return B_NO_MEMORY;

	if (user_memcpy(trimBuffer, buffer, bytes) != B_OK) {
		free(trimBuffer);
		return B_BAD_ADDRESS;
	}

	// The passed in MemoryDeleter needs to take care of freeing the buffer
	// later, since we had to allocate it.
	deleter.SetTo(trimBuffer);

	_trimData = (fs_trim_data*)trimBuffer;
	return B_OK;
}


static inline status_t
copy_trim_data_to_user(void* buffer, fs_trim_data* trimData)
{
	if (!is_called_via_syscall() && !IS_USER_ADDRESS(buffer))
		return B_OK;

	if (!IS_USER_ADDRESS(buffer))
		return B_BAD_ADDRESS;

	// Do not copy any ranges
	return user_memcpy(buffer, trimData, offsetof(fs_trim_data, ranges));
}


#endif	// _FS_TRIM_SUPPORT_H

// SPDX-License-Identifier: MIT
// linux ACPI interface with no namespace behind it, every lookup fails
#ifndef _LINUXKPI_LINUX_ACPI_H_
#define _LINUXKPI_LINUX_ACPI_H_

#include <linux/device.h>
#include <linux/uuid.h>

// no ACPI namespace is reachable from here, every lookup reports failure
typedef void *acpi_handle;
typedef unsigned int acpi_status;
typedef char *acpi_string;
typedef unsigned int acpi_object_type;

#define AE_OK 0
#define AE_NOT_FOUND 5
#define ACPI_FAILURE(s) ((s) != AE_OK)
#define ACPI_SUCCESS(s) ((s) == AE_OK)
#define ACPI_TYPE_INTEGER 1
#define ACPI_TYPE_STRING 2
#define ACPI_TYPE_BUFFER 3
#define ACPI_TYPE_PACKAGE 4
#define ACPI_ALLOCATE_BUFFER ((size_t)-1)
#define ACPI_FREE(p) kfree(p)
#define ACPI_HANDLE(dev) ((acpi_handle)NULL)
#define acpi_device_handle(dev) ((acpi_handle)NULL)
#define acpi_handle_debug(handle, fmt, ...)

union acpi_object {
	acpi_object_type type;
	struct { acpi_object_type type; uint64_t value; } integer;
	struct { acpi_object_type type; uint32_t length; char *pointer; } string;
	struct { acpi_object_type type; uint32_t length; uint8_t *pointer; } buffer;
	struct { acpi_object_type type; uint32_t count; union acpi_object *elements; } package;
};

struct acpi_buffer {
	size_t length;
	void *pointer;
};

struct acpi_object_list {
	uint32_t count;
	union acpi_object *pointer;
};

static inline acpi_status
acpi_get_handle(acpi_handle parent, const char *path, acpi_handle *ret)
{
	return (AE_NOT_FOUND);
}

static inline acpi_status
acpi_evaluate_object(acpi_handle h, const char *path, struct acpi_object_list *args, struct acpi_buffer *ret)
{
	return (AE_NOT_FOUND);
}

static inline union acpi_object *
acpi_evaluate_dsm(acpi_handle h, const guid_t *guid, uint64_t rev, uint64_t func, union acpi_object *argv4)
{
	return (NULL);
}

static inline bool
acpi_check_dsm(acpi_handle h, const guid_t *guid, uint64_t rev, uint64_t funcs)
{
	return (false);
}

#endif

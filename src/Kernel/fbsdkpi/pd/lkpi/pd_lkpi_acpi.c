// SPDX-License-Identifier: MIT
// acpi for linuxkpi drivers: tables through the seam, the namespace not reachable yet
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <linux/acpi.h>
#include <linux/err.h>
#include <linux/notifier.h>
#include <linux/slab.h>

#include "pd_fbsd_os.h"

// the acpi namespace is not reachable from drivers yet
acpi_handle
pd_acpi_device_handle(struct device *dev)
{
	return NULL;
}

acpi_handle
pd_acpi_dev_handle(struct acpi_device *adev)
{
	return NULL;
}

// amdgpu and radeon read the vbios out of VFCT, instances count from 1 as in acpica
acpi_status
acpi_get_table(const char *signature, uint32_t instance,
    struct acpi_table_header **table)
{
	void *data;
	size_t size;

	*table = NULL;
	if (pdos_acpi_get_table(signature, instance > 0 ? instance - 1 : 0,
	    &data, &size) != 0)
		return AE_NOT_FOUND;
	// a kmalloc copy, so acpi_put_table frees it without knowing the size
	*table = kmemdup(data, size, GFP_KERNEL);
	pdos_acpi_put_table(data, size);
	return *table != NULL ? AE_OK : AE_NO_MEMORY;
}

void
acpi_put_table(struct acpi_table_header *table)
{
	kfree(table);
}

acpi_status
acpi_get_handle(acpi_handle parent, const char *pathname, acpi_handle *ret)
{
	*ret = NULL;
	return AE_NOT_FOUND;
}

acpi_status
acpi_get_name(acpi_handle handle, uint32_t name_type, struct acpi_buffer *ret)
{
	return AE_NOT_FOUND;
}

acpi_status
acpi_evaluate_object(acpi_handle handle, const char *pathname,
    struct acpi_object_list *params, struct acpi_buffer *ret)
{
	return AE_NOT_FOUND;
}

union acpi_object *
acpi_evaluate_dsm_typed(acpi_handle handle, const guid_t *guid, uint64_t rev,
    uint64_t func, union acpi_object *argv4, acpi_object_type type)
{
	return NULL;
}

const char *
acpi_format_exception(acpi_status status)
{
	return status == AE_NOT_FOUND ? "AE_NOT_FOUND" : "AE_ERROR";
}

struct acpi_device *
acpi_dev_get_first_match_dev(const char *hid, const char *uid, int64_t hrv)
{
	return NULL;
}

void
acpi_dev_put(struct acpi_device *adev)
{
}

int
register_acpi_notifier(struct notifier_block *nb)
{
	return 0;
}

int
unregister_acpi_notifier(struct notifier_block *nb)
{
	return 0;
}


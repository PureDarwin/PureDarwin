// SPDX-License-Identifier: MIT
// the kernel pieces nouveau reaches for that PD answers itself

#include <linux/types.h>
#include <linux/errno.h>
#include <soc/tegra/fuse.h>

struct thread;

// read by nouveau's tegra path, zero until PD runs on a tegra soc
struct tegra_sku_info tegra_sku_info;

// request_module asks FreeBSD's linker for a kld, PD has none to give
int
kern_kldload(struct thread *td, const char *file, int *fileid)
{
	return ENOENT;
}

// libkern has no strcspn, nvkm's option parser needs it
size_t
strcspn(const char *s, const char *reject)
{
	const char *p, *r;

	for (p = s; *p != '\0'; p++) {
		for (r = reject; *r != '\0'; r++) {
			if (*p == *r)
				return p - s;
		}
	}
	return p - s;
}

#include <linux/acpi.h>

// the amdgpu kext answers this one for itself, nouveau needs its own
acpi_status
acpi_evaluate_integer(acpi_handle handle, const char *pathname,
    struct acpi_object_list *params, unsigned long long *ret)
{
	return AE_NOT_FOUND;
}

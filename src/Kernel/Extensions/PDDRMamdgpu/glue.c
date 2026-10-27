// SPDX-License-Identifier: MIT
// amdgpu registers its pci driver through linuxkpi at sysinit time

#include <sys/bus.h>

void pd_lkpi_init_hardware(void);
extern int (*pd_driver_vsnprintf)(char *, size_t, const char *, va_list);
int pd_drm_vsnprintf(char *, size_t, const char *, va_list);

HAIKU_FBSD_DRIVERS_GLUE(amdgpu);
HAIKU_DRIVER_REQUIREMENTS(FBSD_TASKQUEUES);
NO_HAIKU_FBSD_MII_DRIVER();
NO_HAIKU_REENABLE_INTERRUPTS();

// linuxkpi handlers run on haiku's interrupt thread, amdgpu uses one msi
int
HAIKU_CHECK_DISABLE_INTERRUPTS(device_t dev)
{
	return 1;
}

int pdos_bootarg_int(const char *name, int def);
extern unsigned int amdgpu_dc_debug_mask;

void
__haiku_init_hardware()
{
	// boot-arg amdgpu_dcdebugmask=N is amdgpu's dcdebugmask, display features to leave off
	amdgpu_dc_debug_mask = (unsigned int)pdos_bootarg_int("amdgpu_dcdebugmask", (int)amdgpu_dc_debug_mask);
	// boot-arg drm_debug=N sets drm's debug categories (0x2 driver, 0x4 kms) before amdgpu probes
	{
		extern unsigned long __drm_debug;

		__drm_debug = (unsigned long)pdos_bootarg_int("drm_debug", (int)__drm_debug);
	}
	// device_printf (dev_err, dev_info) learns %pV and the other linux %p forms
	pd_driver_vsnprintf = pd_drm_vsnprintf;
	pd_lkpi_init_hardware();
}

// no network interface comes out of this driver
void pdos_net_input(void *ifp_cookie, const void *frame, size_t len) { }
void pdos_net_input_chain(void *ifp_cookie, void *fbsd_mbuf) { }
void pdos_net_link(void *ifp_cookie, int up, uint64_t baudrate) { }

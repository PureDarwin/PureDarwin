// SPDX-License-Identifier: MIT
// i915 registers its pci driver through linuxkpi at sysinit time

#include <sys/bus.h>

void pd_lkpi_init_hardware(void);
extern int (*pd_driver_vsnprintf)(char *, size_t, const char *, va_list);
int pd_drm_vsnprintf(char *, size_t, const char *, va_list);
void pd_i915_selftest(void);

HAIKU_FBSD_DRIVERS_GLUE(i915);
HAIKU_DRIVER_REQUIREMENTS(FBSD_TASKQUEUES);
NO_HAIKU_FBSD_MII_DRIVER();
NO_HAIKU_REENABLE_INTERRUPTS();

// linuxkpi handlers run on haiku's interrupt thread, i915 uses one msi
int
HAIKU_CHECK_DISABLE_INTERRUPTS(device_t dev)
{
	return 1;
}

int printf(const char *, ...);

void
__haiku_init_hardware()
{
	// device_printf (dev_err, dev_info) learns %pV and the other linux %p forms
	pd_driver_vsnprintf = pd_drm_vsnprintf;
	// shows the aux KC got this far, matching comes after
	printf("PDDRMi915: kext loaded, matching gen6 to gen9 and gen12+ intel graphics\n");
	pd_i915_selftest();
	pd_lkpi_init_hardware();
}

// no network interface comes out of this driver
void pdos_net_input(void *ifp_cookie, const void *frame, size_t len) { }
void pdos_net_input_chain(void *ifp_cookie, void *fbsd_mbuf) { }
void pdos_net_link(void *ifp_cookie, int up, uint64_t baudrate) { }

// SPDX-License-Identifier: MIT
// nouveau registers its pci driver through linuxkpi at sysinit time

#include <sys/bus.h>

void pd_lkpi_init_hardware(void);
extern int (*pd_driver_vsnprintf)(char *, size_t, const char *, va_list);
int pd_drm_vsnprintf(char *, size_t, const char *, va_list);
void pd_nouveau_mmap_register(void);
#ifdef __aarch64__
void pd_tegra210_gpu_init(void);
#endif

HAIKU_FBSD_DRIVERS_GLUE(nouveau);
HAIKU_DRIVER_REQUIREMENTS(FBSD_TASKQUEUES);
NO_HAIKU_FBSD_MII_DRIVER();
NO_HAIKU_REENABLE_INTERRUPTS();

// linuxkpi handlers run on haiku's interrupt thread, nouveau uses one msi
int
HAIKU_CHECK_DISABLE_INTERRUPTS(device_t dev)
{
	return 1;
}

int printf(const char *, ...);

// a soc's memory controller report for nouveau's falcon boot, set by the tegra210 glue
void (*pd_flcn_mc_dump)(const char *);

void
__haiku_init_hardware()
{
	// device_printf (dev_err, dev_info) learns %pV and the other linux %p forms
	pd_driver_vsnprintf = pd_drm_vsnprintf;
	// /dev/dri mmap finds nouveau's buffers through this
	pd_nouveau_mmap_register();
	// shows the aux KC got this far, matching comes after
	printf("PDDRMnouveau: kext loaded, matching nvidia graphics from nv50 on\n");
#ifdef __aarch64__
	// gm20b's clocks, reset and supply, before the platform probe asks for them
	pd_tegra210_gpu_init();
#endif
	pd_lkpi_init_hardware();
}

// no network interface comes out of this driver
void pdos_net_input(void *ifp_cookie, const void *frame, size_t len) { }
void pdos_net_input_chain(void *ifp_cookie, void *fbsd_mbuf) { }
void pdos_net_link(void *ifp_cookie, int up, uint64_t baudrate) { }

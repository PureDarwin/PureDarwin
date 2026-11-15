// glue.c - PDDRMnvidia's linuxkpi side: a pci driver that only claims the GPU's pci_dev for nvidia-drm, and its start
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause

#include <sys/bus.h>
#include <linux/module.h>
#include <linux/pci.h>

void pd_lkpi_init_hardware(void);
extern int (*pd_driver_vsnprintf)(char *, size_t, const char *, va_list);
int pd_drm_vsnprintf(char *, size_t, const char *, va_list);
int nv_drm_init(void);
extern bool nv_drm_modeset_module_param;
extern bool nv_drm_fbdev_module_param;

HAIKU_FBSD_DRIVERS_GLUE(nvidia_drm);
HAIKU_DRIVER_REQUIREMENTS(FBSD_TASKQUEUES);
NO_HAIKU_FBSD_MII_DRIVER();
NO_HAIKU_REENABLE_INTERRUPTS();

// the resource manager in PDNVLoader takes the GPU's interrupts, nothing here does
int
HAIKU_CHECK_DISABLE_INTERRUPTS(device_t dev)
{
	return 0;
}

int printf(const char *, ...);

// the pci_dev of the GPU being attached, set by the probe below
static struct pci_dev *sClaimed;

static int
pd_nvdrm_probe(struct pci_dev *pdev, const struct pci_device_id *id)
{
	sClaimed = pdev;
	return 0;
}

static void
pd_nvdrm_remove(struct pci_dev *pdev)
{
}

// any NVIDIA function: the attach is only ever pointed at a GPU PDNVLoader already drives
static const struct pci_device_id pd_nvdrm_ids[] = {
	{ .vendor = 0x10de, .device = PCI_ANY_ID, .subvendor = PCI_ANY_ID, .subdevice = PCI_ANY_ID },
	{ 0 }
};

static struct pci_driver pd_nvdrm_driver = {
	.name = "nvidia-drm",
	.id_table = pd_nvdrm_ids,
	.probe = pd_nvdrm_probe,
	.remove = pd_nvdrm_remove,
};

static int __init
pd_nvdrm_module_init(void)
{
	return pci_register_driver(&pd_nvdrm_driver);
}
module_init(pd_nvdrm_module_init);

void
__haiku_init_hardware()
{
	pd_driver_vsnprintf = pd_drm_vsnprintf;
	pd_lkpi_init_hardware();
}

// the linux device behind the pci_dev the last attach claimed, for PDNVLoader to report as the GPU's OS device
void *
pd_nvdrm_claimed_device(void)
{
	struct pci_dev *pdev = sClaimed;

	sClaimed = NULL;
	return pdev != NULL ? &pdev->dev : NULL;
}

// as modeset=1 fbdev=0 on linux: kernel modesetting, with PD's display client in place of nvidia-drm's fbdev console
int
pd_nvdrm_start(void)
{
	nv_drm_modeset_module_param = true;
	nv_drm_fbdev_module_param = false;
	return nv_drm_init();
}

// no network interface comes out of this driver
void pdos_net_input(void *ifp_cookie, const void *frame, size_t len) { }
void pdos_net_input_chain(void *ifp_cookie, void *fbsd_mbuf) { }
void pdos_net_link(void *ifp_cookie, int up, uint64_t baudrate) { }

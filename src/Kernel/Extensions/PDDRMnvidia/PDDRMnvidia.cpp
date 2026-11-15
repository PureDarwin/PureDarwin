// PDDRMnvidia.cpp - starts nvidia-drm over the modeset core once PDNVLoader has the GPUs, one linux pci_dev for each
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause

#include <IOKit/IOLib.h>
#include <IOKit/IOService.h>
#include <IOKit/pci/IOPCIDevice.h>

#include "../../fbsdkpi/pd/pd_fbsd_attach.h"
#include "../../fbsdkpi/xnu/pd_fbsd_xnu.h"

extern "C" {
// PDNVLoader's, which this kext links against
uint32_t pdnv_gpu_count(void);
void *pdnv_gpu_pci_device(uint32_t i);
void pdnv_gpu_set_os_device(uint32_t i, void *os_device);
void *pd_nvdrm_claimed_device(void);
int pd_nvdrm_start(void);
}

class PDDRMnvidia : public IOService {
	OSDeclareDefaultStructors(PDDRMnvidia);

public:
	bool start(IOService *provider) override;
};

OSDefineMetaClassAndStructors(PDDRMnvidia, IOService);

bool
PDDRMnvidia::start(IOService *provider)
{
	uint32_t n, i, linked = 0;

	if (!IOService::start(provider))
		return false;
	pdos_xnu_init();

	// PDNVGPU holds each IOPCIDevice open, the linux side only reads its config space and BARs through it
	n = pdnv_gpu_count();
	for (i = 0; i < n; i++) {
		IOPCIDevice *pci = OSDynamicCast(IOPCIDevice, (OSObject *)pdnv_gpu_pci_device(i));
		void *dev;

		if (pci == NULL || pd_fbsd_attach_pci_device(pci) == NULL ||
		    (dev = pd_nvdrm_claimed_device()) == NULL) {
			IOLog("PDDRMnvidia: GPU %u has no linux pci device\n", i);
			continue;
		}
		pdnv_gpu_set_os_device(i, dev);
		linked++;
	}
	if (linked == 0)
		return false;

	int err = pd_nvdrm_start();
	IOLog("PDDRMnvidia: nv_drm_init returned %d for %u GPU(s)\n", err, linked);
	if (err != 0)
		return false;
	// PDDRMFramebuffer matches on this, then lights the displays through the drm display client
	registerService();
	return true;
}

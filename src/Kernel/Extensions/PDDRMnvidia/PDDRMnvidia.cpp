// PDDRMnvidia.cpp - nvidia-drm over the modeset core, one instance per GPU PDNVLoader has brought up, each with its linux pci_dev
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause

#include <IOKit/IOLib.h>
#include <IOKit/IOService.h>
#include <IOKit/pci/IOPCIDevice.h>

#include "../../fbsdkpi/pd/pd_fbsd_attach.h"
#include "../../fbsdkpi/xnu/pd_fbsd_xnu.h"

extern "C" {
// PDNVLoader's, which this kext links against
uint32_t pdnv_gpu_count(void);
uint32_t pdnv_gpu_id(uint32_t i);
void *pdnv_gpu_pci_device(uint32_t i);
void pdnv_gpu_set_os_device(uint32_t i, void *os_device);
void *pd_nvdrm_claimed_device(void);
int pd_nvdrm_add_gpu(uint32_t gpu_id);
}

class PDDRMnvidia : public IOService {
	OSDeclareDefaultStructors(PDDRMnvidia);

public:
	bool start(IOService *provider) override;
};

OSDefineMetaClassAndStructors(PDDRMnvidia, IOService);

// one start at a time: nvidia-drm comes up once, then takes the GPUs one by one
static IOLock *sStartLock;

bool
PDDRMnvidia::start(IOService *provider)
{
	if (!IOService::start(provider))
		return false;
	pdos_xnu_init();

	// the provider is PDNVGPU, registered once its GPU is up, on the IOPCIDevice it holds open: the linux side
	// only reads that device's config space and BARs
	IOPCIDevice *pci = OSDynamicCast(IOPCIDevice, provider->getProvider());
	uint32_t n = pdnv_gpu_count(), i;
	for (i = 0; i < n && pdnv_gpu_pci_device(i) != pci; i++)
		;
	if (pci == NULL || i == n) {
		IOLog("PDDRMnvidia: %s is not a GPU PDNVLoader drives\n", provider->getName());
		return false;
	}

	if (sStartLock == NULL) {
		IOLock *lock = IOLockAlloc();
		if (!OSCompareAndSwapPtr(NULL, lock, (void * volatile *)&sStartLock))
			IOLockFree(lock);
	}
	IOLockLock(sStartLock);
	void *dev = NULL;
	int err = -1;
	if (pd_fbsd_attach_pci_device(pci) != NULL && (dev = pd_nvdrm_claimed_device()) != NULL) {
		pdnv_gpu_set_os_device(i, dev);
		err = pd_nvdrm_add_gpu(pdnv_gpu_id(i));
	}
	IOLockUnlock(sStartLock);
	if (dev == NULL) {
		IOLog("PDDRMnvidia: GPU %u has no linux pci device\n", i);
		return false;
	}
	IOLog("PDDRMnvidia: GPU %u (id 0x%x) into nvidia-drm, %d\n", i, pdnv_gpu_id(i), err);
	if (err != 0)
		return false;
	// PDDRMFramebuffer matches on this, then lights the displays through the drm display client
	registerService();
	return true;
}

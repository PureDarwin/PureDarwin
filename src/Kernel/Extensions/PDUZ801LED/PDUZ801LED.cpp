// PDUZ801LED.cpp - the uz801 stick's three status leds, set through the "led" registry property
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause

#include <IOKit/IOService.h>
#include <IOKit/IOMemoryDescriptor.h>
#include <IOKit/IODeviceTreeSupport.h>
#include <IOKit/IOLib.h>

// tlmm pins, active high, as the stock firmware's gpio_leds node has them
enum {
	kTlmmBase = 0x01000000,
	kPinStride = 0x1000,
	kPinGreen = 71,
	kPinRed = 72,
	kPinBlue = 73,
	kConfigOutput = 0x200,	// output enable, 2 ma drive, gpio function
	kInOutHigh = 0x2,
};

class PDUZ801LED : public IOService
{
	OSDeclareDefaultStructors(PDUZ801LED);

public:
	bool start(IOService *provider) APPLE_KEXT_OVERRIDE;
	void stop(IOService *provider) APPLE_KEXT_OVERRIDE;
	IOReturn setProperties(OSObject *properties) APPLE_KEXT_OVERRIDE;
	void set(int bits);
	int bits;

private:
	IOMemoryMap *fMap[3];
	volatile uint32_t *fPin[3];
};

OSDefineMetaClassAndStructors(PDUZ801LED, IOService);

bool
PDUZ801LED::start(IOService *provider)
{
	static const int pins[3] = { kPinRed, kPinGreen, kPinBlue };
	IORegistryEntry *root;
	bool ours = false;

	if (!IOService::start(provider))
		return false;

	// xnu-loader gives the root a virtual mac identity, so look for the msm8916 pin controller
	// these leds hang off. the msm8916 kernel collection only targets this stick
	root = IORegistryEntry::fromPath("/", gIODTPlane);
	if (root != NULL) {
		OSIterator *it = IORegistryIterator::iterateOver(root, gIODTPlane, kIORegistryIterateRecursively);
		IORegistryEntry *e;
		while (it != NULL && !ours && (e = OSDynamicCast(IORegistryEntry, it->getNextObject())) != NULL) {
			OSData *compat = OSDynamicCast(OSData, e->getProperty("compatible"));
			ours = compat != NULL && strncmp((const char *)compat->getBytesNoCopy(), "qcom,msm8916-pinctrl",
			    compat->getLength()) == 0;
		}
		OSSafeReleaseNULL(it);
		root->release();
	}
	if (!ours)
		return false;

	for (int i = 0; i < 3; i++) {
		IOMemoryDescriptor *md = IOMemoryDescriptor::withPhysicalAddress(kTlmmBase + pins[i] * kPinStride,
		    8, kIODirectionInOut);
		fMap[i] = md != NULL ? md->createMappingInTask(kernel_task, 0, kIOMapAnywhere | kIOMapInhibitCache) : NULL;
		if (md != NULL)
			md->release();
		if (fMap[i] == NULL)
			return false;
		fPin[i] = (volatile uint32_t *)fMap[i]->getVirtualAddress();
	}

	// the bootloader leaves all three on and they are very bright: start dark
	set(0);
	registerService();
	return true;
}

// "led" = bit 0 red, bit 1 green, bit 2 blue, from userland with IORegistryEntrySetCFProperty
IOReturn
PDUZ801LED::setProperties(OSObject *properties)
{
	OSDictionary *dict = OSDynamicCast(OSDictionary, properties);
	OSNumber *n = dict != NULL ? OSDynamicCast(OSNumber, dict->getObject("led")) : NULL;

	if (n == NULL)
		return kIOReturnBadArgument;
	set(n->unsigned32BitValue() & 7);
	return kIOReturnSuccess;
}

void
PDUZ801LED::set(int newBits)
{
	bits = newBits;
	setProperty("led", (unsigned long long)bits, 32);
	for (int i = 0; i < 3; i++) {
		fPin[i][0] = kConfigOutput;
		fPin[i][1] = (bits >> i) & 1 ? kInOutHigh : 0;
	}
	__asm__ volatile ("dsb sy" ::: "memory");
}

void
PDUZ801LED::stop(IOService *provider)
{
	for (int i = 0; i < 3; i++)
		OSSafeReleaseNULL(fMap[i]);
	IOService::stop(provider);
}

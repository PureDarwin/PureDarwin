/*
 * Portions Copyright (c) 1999-2003 Apple Computer, Inc. All Rights
 * Reserved.
 *
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 *
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 *
 * This file was modified by William Kent in 2017 to support the PureDarwin
 * project. This notice is included in support of clause 2.2(b) of the License.
 */

#include <IOKit/IOLib.h>
#include <IOKit/assert.h>
#include <IOKit/system.h>
#include <IOKit/IORegistryEntry.h>
#include <IOKit/IODeviceTreeSupport.h>   // gIODTPlane
#include <IOKit/IOKitKeys.h>             // kIOPlatformUUIDKey
#include <IOKit/platform/ApplePlatformExpert.h>
#include <libkern/c++/OSContainers.h>
#include <libkern/c++/OSUnserialize.h>
#include <pexpert/i386/boot.h>

extern "C" {
#include <i386/cpuid.h>
#include <pexpert/i386/protos.h>
}

#include "../AppleAPIC/PICShared.h"
#include "PDACPIPlatformExpert.h"

extern "C" {
#include <uacpi/uacpi.h>
#include <uacpi/tables.h>
#include <uacpi/acpi.h>
#include <uacpi/status.h>
}

// kprintf writes straight to the serial console, bypassing os_log (which drops
// IOLog output from prelinked kexts that aren't fully OSKext-registered).
extern "C" void kprintf(const char *fmt, ...);

enum {
	kIRQAvailable   = 0,
	kIRQExclusive   = 1,
	kIRQSharable    = 2,
	kSystemIRQCount = 16
};

static struct {
	UInt16 consumers;
	UInt16 status;
} IRQ[kSystemIRQCount];

static IOLock *ResourceLock;

class PDACPIPlatformExpertGlobals {
public:
	bool isValid;
	PDACPIPlatformExpertGlobals();
	~PDACPIPlatformExpertGlobals();
};

static PDACPIPlatformExpertGlobals PDACPIPlatformExpertGlobals;
PDACPIPlatformExpertGlobals::PDACPIPlatformExpertGlobals() {
	ResourceLock = IOLockAlloc();
	bzero(IRQ, sizeof(IRQ));
}

PDACPIPlatformExpertGlobals::~PDACPIPlatformExpertGlobals() {
	if (ResourceLock) IOLockFree(ResourceLock);
}

#pragma mark -

#define super IOACPIPlatformExpert

OSDefineMetaClassAndStructors(PDACPIPlatformExpert, IOACPIPlatformExpert);

// Must match nothing: NULL makes IODTFindMatchingEntries detach the whole tree.
const char *PDACPIPlatformExpert::deleteList(void) {
	return "('pd-delete-none')";
}

const char *PDACPIPlatformExpert::excludeList(void) {
	return NULL;
}

IOService *PDACPIPlatformExpert::probe(IOService *provider, SInt32 *score) {
	if (score != 0) *score = 10000;
	return this;
}

bool PDACPIPlatformExpert::init(OSDictionary *properties) {\
	if (!super::init(properties)) return false;

	OSString *name = (OSString *)getProperty("InterruptControllerName");
	if (name == 0) name = OSString::withCStringNoCopy("AppleI386CPUInterruptController");
	_interruptControllerName = OSSymbol::withString(name);

	return true;
}

/*
 * Publish IOPlatformUUID.
 *
 * IOBSDGetPlatformUUID() answers gethostuuid() by doing
 * waitForService(resourceMatching(kIOPlatformUUIDKey)), and CFPreferences asks
 * with an infinite timeout - so until that resource exists, anything touching
 * preferences (or hw.uuid) blocks forever at 0% CPU rather than failing.
 *
 * On x86_64 stock XNU only publishes it from
 * IOPlatformExpert::registerNVRAMController(), because on a real Mac the UUID
 * comes out of NVRAM. PureDarwin has no IONVRAMController, so that path never
 * runs; xnu-loader instead puts the 16-byte "platform-uuid" property (from
 * SMBIOS, falling back to the boot volume UUID) on the device-tree /options
 * node, and we publish it here.
 */
void PDACPIPlatformExpert::publishPlatformUUIDFromDeviceTree(void) {
	IORegistryEntry *options = IORegistryEntry::fromPath("/options", gIODTPlane);
	if (options == 0) {
		IOLog("PDACPIPlatformExpert: no /options node, IOPlatformUUID unavailable\n");
		return;
	}

	OSData *data = OSDynamicCast(OSData, options->getProperty("platform-uuid"));
	if (data == 0 || data->getLength() != sizeof(uuid_t)) {
		IOLog("PDACPIPlatformExpert: /options has no usable platform-uuid, "
		      "IOPlatformUUID unavailable\n");
		options->release();
		return;
	}

	uuid_string_t uuid;
	uuid_unparse((const unsigned char *)data->getBytesNoCopy(), uuid);
	options->release();

	OSString *string = OSString::withCString(uuid);
	if (string == 0) return;

	/* Both are needed: the property answers IOKit lookups and the sysctl, the
	 * resource is what waitForService() in IOBSDGetPlatformUUID() blocks on. */
	setProperty(kIOPlatformUUIDKey, string);
	publishResource(kIOPlatformUUIDKey, string);
	IOLog("PDACPIPlatformExpert: published IOPlatformUUID %s\n", uuid);
	string->release();
}

UInt16 PDACPIPlatformExpert::sPM1aControlPort = 0;
UInt16 PDACPIPlatformExpert::sPM1bControlPort = 0;
UInt8  PDACPIPlatformExpert::sS5SleepTypeA = 0;
UInt8  PDACPIPlatformExpert::sS5SleepTypeB = 0;
bool   PDACPIPlatformExpert::sACPIPowerOffReady = false;

/* Map a physical range long enough to read a table out of it. ACPI tables sit
 * in EFI reclaim memory, which is normal RAM by the time we run. */
static void *pd_acpi_map(UInt64 phys, UInt32 length, IOMemoryMap **mapOut) {
	IOMemoryDescriptor *md = IOMemoryDescriptor::withPhysicalAddress(
	    (IOPhysicalAddress)phys, length, kIODirectionIn);
	if (md == 0) return 0;
	IOMemoryMap *map = md->map();
	md->release();
	if (map == 0) return 0;
	*mapOut = map;
	return (void *)map->getVirtualAddress();
}

static bool pd_acpi_parse_s5(const UInt8 *dsdt, UInt32 length,
                             UInt8 *typA, UInt8 *typB) {
	if (dsdt == 0 || length < 8) return false;
	for (UInt32 i = 0; i + 8 < length; i++) {
		if (dsdt[i] != '_' || dsdt[i + 1] != 'S' ||
		    dsdt[i + 2] != '5' || dsdt[i + 3] != '_') continue;

		const UInt8 *p = dsdt + i + 4;
		if (*p != 0x12) continue;          // PackageOp
		p++;
		p += ((*p & 0xC0) >> 6) + 1;       // skip PkgLength
		if (p >= dsdt + length) return false;
		if (*p < 2) return false;          // need at least two elements
		p++;

		/* Each element is either a byte-prefixed integer (0x0A <val>) or a
		 * bare Zero/One/Ones constant. */
		if (*p == 0x0A) p++;
		if (p >= dsdt + length) return false;
		*typA = *p++;
		if (*p == 0x0A) p++;
		if (p >= dsdt + length) return false;
		*typB = *p;
		return true;
	}
	return false;
}

void PDACPIPlatformExpert::cacheACPIPowerOffFromDeviceTree(void) {
	IORegistryEntry *cfg =
	    IORegistryEntry::fromPath("/efi/configuration-table", gIODTPlane);
	if (cfg == 0) {
		IOLog("PDACPIPlatformExpert: no /efi/configuration-table, "
		      "ACPI power off unavailable\n");
		return;
	}

	/* Prefer the ACPI 2.0 entry: its RSDP carries the 64-bit XSDT. */
	UInt64 rsdpPhys = 0;
	bool haveV2 = false;
	OSIterator *children = cfg->getChildIterator(gIODTPlane);
	if (children != 0) {
		while (OSObject *next = children->getNextObject()) {
			IORegistryEntry *child = OSDynamicCast(IORegistryEntry, next);
			if (child == 0) continue;
			OSData *alias = OSDynamicCast(OSData, child->getProperty("alias"));
			OSData *table = OSDynamicCast(OSData, child->getProperty("table"));
			if (alias == 0 || table == 0 || table->getLength() < 8) continue;
			const char *a = (const char *)alias->getBytesNoCopy();
			bool isV2 = (strncmp(a, "ACPI_20", 7) == 0);
			if (!isV2 && strncmp(a, "ACPI", 4) != 0) continue;
			if (haveV2 && !isV2) continue;
			rsdpPhys = *(const UInt64 *)table->getBytesNoCopy();
			haveV2 = isV2;
		}
		children->release();
	}
	cfg->release();

	if (rsdpPhys == 0) {
		IOLog("PDACPIPlatformExpert: no ACPI RSDP in the device tree, "
		      "ACPI power off unavailable\n");
		return;
	}

	IOMemoryMap *rsdpMap = 0;
	const UInt8 *rsdp = (const UInt8 *)pd_acpi_map(rsdpPhys, 36, &rsdpMap);
	if (rsdp == 0) return;

	UInt8 revision = rsdp[15];
	UInt64 sdtPhys = 0;
	bool useXsdt = false;
	if (revision >= 2) {
		sdtPhys = *(const UInt64 *)(rsdp + 24);   // XsdtAddress
		useXsdt = (sdtPhys != 0);
	}
	if (!useXsdt) sdtPhys = *(const UInt32 *)(rsdp + 16);  // RsdtAddress
	rsdpMap->release();
	if (sdtPhys == 0) return;

	/* Read the header first for the real length, then remap the whole table. */
	IOMemoryMap *sdtHdrMap = 0;
	const UInt8 *sdtHdr = (const UInt8 *)pd_acpi_map(sdtPhys, 36, &sdtHdrMap);
	if (sdtHdr == 0) return;
	UInt32 sdtLen = *(const UInt32 *)(sdtHdr + 4);
	sdtHdrMap->release();
	if (sdtLen < 36 || sdtLen > (1u << 20)) return;

	IOMemoryMap *sdtMap = 0;
	const UInt8 *sdt = (const UInt8 *)pd_acpi_map(sdtPhys, sdtLen, &sdtMap);
	if (sdt == 0) return;

	UInt64 fadtPhys = 0;
	UInt32 entrySize = useXsdt ? 8 : 4;
	UInt32 entries = (sdtLen - 36) / entrySize;
	for (UInt32 i = 0; i < entries && fadtPhys == 0; i++) {
		const UInt8 *e = sdt + 36 + (i * entrySize);
		UInt64 tablePhys = useXsdt ? *(const UInt64 *)e : *(const UInt32 *)e;
		if (tablePhys == 0) continue;
		IOMemoryMap *hdrMap = 0;
		const UInt8 *hdr = (const UInt8 *)pd_acpi_map(tablePhys, 36, &hdrMap);
		if (hdr == 0) continue;
		if (strncmp((const char *)hdr, "FACP", 4) == 0) fadtPhys = tablePhys;
		hdrMap->release();
	}
	sdtMap->release();

	if (fadtPhys == 0) {
		IOLog("PDACPIPlatformExpert: no FACP table, "
		      "ACPI power off unavailable\n");
		return;
	}

	IOMemoryMap *fadtMap = 0;
	const UInt8 *fadt = (const UInt8 *)pd_acpi_map(fadtPhys, 244, &fadtMap);
	if (fadt == 0) return;

	sPM1aControlPort = (UInt16)*(const UInt32 *)(fadt + 64);   // PM1a_CNT_BLK
	sPM1bControlPort = (UInt16)*(const UInt32 *)(fadt + 68);   // PM1b_CNT_BLK
	UInt64 dsdtPhys = *(const UInt32 *)(fadt + 40);            // DSDT
	fadtMap->release();

	if (sPM1aControlPort == 0) {
		IOLog("PDACPIPlatformExpert: FACP has no PM1a control block, "
		      "ACPI power off unavailable\n");
		return;
	}

	/* Sleep type defaults to 0, which is what QEMU's \_S5 uses; a DSDT parse
	 * only has to correct it on hardware that differs. */
	if (dsdtPhys != 0) {
		IOMemoryMap *dsdtHdrMap = 0;
		const UInt8 *dsdtHdr =
		    (const UInt8 *)pd_acpi_map(dsdtPhys, 36, &dsdtHdrMap);
		if (dsdtHdr != 0) {
			UInt32 dsdtLen = *(const UInt32 *)(dsdtHdr + 4);
			dsdtHdrMap->release();
			if (dsdtLen >= 36 && dsdtLen <= (1u << 22)) {
				IOMemoryMap *dsdtMap = 0;
				const UInt8 *dsdt =
				    (const UInt8 *)pd_acpi_map(dsdtPhys, dsdtLen, &dsdtMap);
				if (dsdt != 0) {
					(void)pd_acpi_parse_s5(dsdt, dsdtLen,
					    &sS5SleepTypeA, &sS5SleepTypeB);
					dsdtMap->release();
				}
			}
		}
	}

	sACPIPowerOffReady = true;
	IOLog("PDACPIPlatformExpert: ACPI power off via PM1a 0x%x "
	      "(PM1b 0x%x), S5 type %u/%u\n", sPM1aControlPort, sPM1bControlPort,
	      sS5SleepTypeA, sS5SleepTypeB);
}

bool PDACPIPlatformExpert::start(IOService *provider) {
	setBootROMType(kBootROMTypeNewWorld);

	bool superOK = super::start(provider);
	if (!superOK) return false;
	PE_halt_restart = handlePEHaltRestart;
	registerService();

	publishPlatformUUIDFromDeviceTree();
	cacheACPIPowerOffFromDeviceTree();

	// After the platform duties: a uACPI failure must not cost us the expert.
	if (PDACPIGlueInit() && setupEarlyTables()) {
		fUACPIStarted = true;
		kprintf("PDACPIPlatform: ACPI tables available\n");
		logTable(ACPI_FADT_SIGNATURE);
		logTable(ACPI_MADT_SIGNATURE);
		logTable(ACPI_MCFG_SIGNATURE);
		logTable(ACPI_HPET_SIGNATURE);
	} else {
		kprintf("PDACPIPlatform: uACPI table access unavailable\n");
	}

	// Must precede the interrupt controller: initCPUInterruptController()
	// calls ml_set_max_cpus(), which is what hw.ncpu reports.
	fCPUCount = enumerateProcessorsFromMADT();
	if (fCPUCount == 0) fCPUCount = 1;   // boot processor only

	// Hack: Initialize AppleI386CPU ourself because no one else will.
	bootCPU = new AppleI386CPU;
	if (bootCPU == 0) return false;

	bootCPU->init();
	bootCPU->attach(0);
	if (!bootCPU->startCommon(fCPUCount)) return false;

	if (fCPUCount > 1) registerProcessors();

	return true;
}

bool PDACPIPlatformExpert::configure(IOService *provider) {
	OSArray *topLevel;
	OSDictionary *dict;
	IOService *nub;

	topLevel = OSDynamicCast(OSArray, getProperty("top-level"));

	if (topLevel) {
		unsigned int count = topLevel->getCount();
		for (unsigned int i = 0; i < count; i++) {
			dict = OSDynamicCast(OSDictionary, topLevel->getObject(i));
			if (dict == 0) continue;

			nub = createNub(dict);
			if (nub == 0) { kprintf(">>>   createNub -> NULL\n"); continue; }

			nub->attach(this);

			/*
			 * Also root the nub in the IODeviceTree plane, under the
			 * device-tree entry that plane hangs off.
			 */
			if (provider && !nub->attachToParent(provider, gIODTPlane)) {
				kprintf(">>>   nub '%s': attachToParent(gIODTPlane) failed\n",
				        nub->getName());
			}

			nub->registerService();
			kprintf(">>>   registered nub '%s'\n", nub->getName());
			nub->release();
		}
	}

	return true;
}

bool PDACPIPlatformExpert::matchNubWithPropertyTable(IOService *nub, OSDictionary *table) {
	OSString *nameProp;
	OSString *match;

	if ((nameProp = (OSString *)nub->getProperty(gIONameKey)) == 0) return false;
	if ((match = (OSString *)table->getObject(gIONameMatchKey)) == 0) return false;

	return match->isEqualTo(nameProp);
}

IOService *PDACPIPlatformExpert::createNub(OSDictionary *from) {
	IOService *nub;

	// Named explicitly: IODTPlatformExpert introduces createNub(IORegistryEntry*),
	// which hides the OSDictionary form this synthesises nubs with.
	nub = IOPlatformExpert::createNub(from);
	if (nub) {
		const char *name = nub->getName();

		if (strcmp(name, "pci") == 0) {
			// TODO: Get the PCI info from the boot args
			// and set it as the `pci-bus-info` property in the `from` dict.
		} else if (strcmp(name, "bios") == 0) {
			setupBIOS(nub);
		} else if (strcmp(name, "8259-pic") == 0) {
			setupPIC(nub);
		} else if (strcmp(name, "ps2controller") == 0) {
			// ApplePS2Controller calls registerInterrupt(1/12) on this nub,
			// so it needs the same legacy-IRQ specifier table as the PIC.
			setupPIC(nub);
		}
	}

	return nub;
}

void PDACPIPlatformExpert::setupPIC(IOService *nub) {
	int i;
	OSDictionary *propTable;
	OSArray *controller;
	OSArray *specifier;
	OSData *tmpData;

	propTable = nub->getPropertyTable();

	// For the moment... assume a classic 8259 interrupt controller
	// with 16 interrupts. Later, this will be changed to detect
	// an APIC and/or MP-Table and then will set the nubs appropriately.

	specifier = OSArray::withCapacity(kSystemIRQCount);
	assert(specifier);

	for (i = 0; i < kSystemIRQCount; i++) {
		UInt32 spec[2];
		spec[0] = i;
		spec[1] = kInterruptTriggerModeEdge |
		    kInterruptPolarityHigh |
		    kInterruptNotShareable;
		tmpData = OSData::withBytes(spec, sizeof(spec));
		specifier->setObject(tmpData);
		tmpData->release();
	}

	controller = OSArray::withCapacity(kSystemIRQCount);
	assert(controller);

	for (i = 0; i < kSystemIRQCount; i++) controller->setObject(_interruptControllerName);

	propTable->setObject(gIOInterruptControllersKey, controller);
	propTable->setObject(gIOInterruptSpecifiersKey, specifier);

	specifier->release();
	controller->release();
}

void PDACPIPlatformExpert::setupBIOS(IOService *nub) {
	// TODO: Implement this function.
	// This function is dependent upon being able to retrieve the
	// PCI bus data. While the booter does collect some PCI data,
	// but it does not include the data needed here.
}

bool PDACPIPlatformExpert::getMachineName(char *name, int maxLength) {
	if (!name || maxLength <= 0) {
		return false;
	}

#if defined(__x86_64__)
	const char *machineName = "x86_64";
#elif defined(__i386__)
	const char *machineName = "i386";
#else
	const char *machineName = "x86";
#endif

	strncpy(name, machineName, maxLength);
	name[maxLength - 1] = '\0';
	return true;
}

bool PDACPIPlatformExpert::getModelName(char *name, int maxLengh) {
	i386_cpu_info_t *cpuid_cpu_info = cpuid_info();

	if (cpuid_cpu_info->cpuid_brand_string[0] != '\0') {
		strncpy(name, cpuid_cpu_info->cpuid_brand_string, maxLengh);
	} else {
		strncpy(name, cpuid_cpu_info->cpuid_model_string, maxLengh);
	}

	return true;
}

int PDACPIPlatformExpert::handlePEHaltRestart(unsigned int type) {
	int ret = -1;
	int temporary_sum = 0;

	switch (type) {
		case kPERestartCPU:
			// Note: This code may or may not work reliably on all systems.
			// The original author of it indicated that it should work on any
			// system with a compliant PCI controller.

			// Obtained from: http://smackerelofopinion.blogspot.nl/2009/06/rebooting-pc.html
			outb(0xCF9, 0x02);

			// A delay of some sort is required here.
			temporary_sum = 2;
			temporary_sum += 2;

			outb(0xCF9, 0x04);

			// This should not be reached, but just in case...
			break;

		case kPEHaltCPU:
			/* ACPI soft-off: write SLP_TYP|SLP_EN to the PM1 control
			 * register(s). Without this the caller (halt_all_cpus) just spins
			 * in a while(1), so the machine never actually powers down. */
			if (sACPIPowerOffReady) {
				outw(sPM1aControlPort,
				    (UInt16)((sS5SleepTypeA << 10) | 0x2000));
				if (sPM1bControlPort != 0) {
					outw(sPM1bControlPort,
					    (UInt16)((sS5SleepTypeB << 10) | 0x2000));
				}
				/* Power removal is not instantaneous; give it a moment before
				 * admitting failure to the caller. */
				for (temporary_sum = 0; temporary_sum < 100000; temporary_sum++) {
					outb(0x80, 0);
				}
			}

			IOLog("PDACPIPlatformExpert: ACPI power off unavailable, "
			      "resetting instead\n");
			outb(0xCF9, 0x02);
			temporary_sum = 2;
			temporary_sum += 2;
			outb(0xCF9, 0x04);
			ret = -1;
			break;

		default:
			ret = -1;
			break;
	}

	return ret;
}

bool PDACPIPlatformExpert::setNubInterruptVectors(IOService *nub, const UInt32 *vectors, UInt32 vectorCount) {
	OSArray *controller = 0;
	OSArray *specifier = 0;
	bool success = false;

	if (vectorCount == 0) {
		nub->removeProperty(gIOInterruptControllersKey);
		nub->removeProperty(gIOInterruptSpecifiersKey);
		return true;
	}

	specifier = OSArray::withCapacity(vectorCount);
	controller = OSArray::withCapacity(vectorCount);
	if (!specifier || !controller) goto done;

	for (UInt32 i = 0; i < vectorCount; i++) {
		// The interrupt specifier must be an 8-byte blob: word[0] = vector/IRQ
		// number (IOAPIC input pin), word[1] = interrupt flags.  AppleAPIC's
		// IOAPIC controller (AppleAPICInterruptController::getInterruptType)
		// rejects any specifier shorter than sizeof(UInt64), which is why a
		// bare 4-byte UInt32 caused every device interrupt registration to
		// fail with kIOReturnNotFound.  Legacy ISA IRQs (e.g. IDE 14/15) are
		// edge-triggered, active-high, non-shareable -> flags = 0.
		UInt32 spec[2];
		spec[0] = vectors[i];
		spec[1] = 0;  // kInterruptTriggerModeEdge | kInterruptPolarityHigh | kInterruptNotShareable
		OSData *data = OSData::withBytes(spec, sizeof(spec));
		specifier->setObject(data);
		controller->setObject(_interruptControllerName);
		if (data) data->release();
	}

	nub->setProperty(gIOInterruptControllersKey, controller);
	nub->setProperty(gIOInterruptSpecifiersKey, specifier);
	success = true;

done:
	if (specifier) specifier->release();
	if (controller) controller->release();
	return success;
}

bool PDACPIPlatformExpert::setNubInterruptVector(IOService *nub, UInt32 vector) {
	return setNubInterruptVectors(nub, &vector, 1);
}

IOReturn PDACPIPlatformExpert::callPlatformFunction(const OSSymbol *functionName, bool waitForFunction, void *param1, void *param2, void *param3, void *param4) {
	bool ok;

	if (functionName->isEqualTo("SetDeviceInterrupts")) {
		IOService *nub = (IOService *)param1;
		UInt32 *vectors = (UInt32 *)param2;
		UInt32 vectorCount = (UInt32)((UInt64)param3);
		bool exclusive = (bool)param4;

		if (vectorCount != 1) return kIOReturnBadArgument;

		ok = reserveSystemInterrupt(nub, vectors[0], exclusive);
		if (ok == false) return kIOReturnNoResources;

		ok = setNubInterruptVector(nub, vectors[0]);
		if (ok == false) releaseSystemInterrupt(nub, vectors[0], exclusive);

		return ok ? kIOReturnSuccess : kIOReturnNoMemory;
	} else if (functionName->isEqualTo("GetMessagedInterruptAddress")) {
		// Build the x86 MSI message for a message-signalled interrupt targeting
		// the boot CPU's local APIC. Called (via provider-chain propagation) by
		// IOPCIMessagedInterruptController::allocateDeviceInterrupts.
		//   param3 = interrupt vector (already includes the controller vector base)
		//   param4 = uint32_t message[3] out: { addr-lo, addr-hi, data }
		uint32_t   vector  = (uint32_t)((UInt64)param3);
		uint32_t * message = (uint32_t *)param4;
		if (message == 0) return kIOReturnBadArgument;

		// Destination = boot CPU local APIC ID (0), matching the io-apic nub's
		// "Destination APIC ID" personality. Physical destination mode, no
		// redirection hint. Data: fixed delivery mode, edge triggered, vector.
		const uint32_t destAPICID = 0;
		message[0] = 0xFEE00000U | (destAPICID << 12);  // MSI address low
		message[1] = 0;                                 // MSI address high
		message[2] = vector & 0xFFU;                    // MSI data
		kprintf("PDACPIPlatformExpert: MSI msg vector=0x%x addr=0x%08x data=0x%x\n",
			vector, message[0], message[2]);
		return kIOReturnSuccess;
	} else if (functionName->isEqualTo("SetBusClockRateMHz")) {
		UInt32 rateMHz = (UInt32)((UInt64)param1);
		gPEClockFrequencyInfo.bus_clock_rate_hz = rateMHz * 1000000;
		return kIOReturnSuccess;
	} else if (functionName->isEqualTo("SetCPUClockRateMHz")) {
		UInt32 rateMHz = (UInt32)((UInt64)param1);
		gPEClockFrequencyInfo.cpu_clock_rate_hz = rateMHz * 1000000;
		return kIOReturnSuccess;
	}

	return super::callPlatformFunction(functionName, waitForFunction, param1, param2, param3, param4);
}

bool PDACPIPlatformExpert::reserveSystemInterrupt(IOService *client, UInt32 vectorNumber, bool exclusive) {
	bool ok = false;
	if (vectorNumber >= kSystemIRQCount) return ok;

	IOLockLock(ResourceLock);

	if (exclusive) {
		if (IRQ[vectorNumber].status == kIRQAvailable) {
			IRQ[vectorNumber].status = kIRQExclusive;
			IRQ[vectorNumber].consumers = 1;
			ok = true;
		}
	} else {
		if (IRQ[vectorNumber].status == kIRQAvailable || IRQ[vectorNumber].status == kIRQSharable) {
			IRQ[vectorNumber].status = kIRQSharable;
			IRQ[vectorNumber].consumers++;
			ok = true;
		}
	}

	IOLockUnlock(ResourceLock);
	return ok;
}

void PDACPIPlatformExpert::releaseSystemInterrupt(IOService *client, UInt32 vectorNumber, bool exclusive) {
	if (vectorNumber >= kSystemIRQCount) return;
	IOLockLock(ResourceLock);

	if (exclusive) {
		if (IRQ[vectorNumber].status == kIRQExclusive) {
			IRQ[vectorNumber].status = kIRQAvailable;
			IRQ[vectorNumber].consumers = 0;
		}
	} else {
		if (IRQ[vectorNumber].status == kIRQSharable && --IRQ[vectorNumber].consumers == 0) {
			IRQ[vectorNumber].status = kIRQAvailable;
		}
	}

	IOLockUnlock(ResourceLock);
}

#define CMOS_ADDR_PORT  0x70
#define CMOS_DATA_PORT  0x71

#define RTC_SECONDS     0x00
#define RTC_MINUTES     0x02
#define RTC_HOURS       0x04
#define RTC_DAY_OF_MONTH 0x07
#define RTC_MONTH       0x08
#define RTC_YEAR        0x09
#define RTC_STATUS_A    0x0A
#define RTC_STATUS_B    0x0B

#define RTC_STATUS_A_UPDATE_IN_PROGRESS 0x80

#define RTC_STATUS_B_24_HOUR    0x02
#define RTC_STATUS_B_BINARY     0x04

static uint8_t
rtcRead(uint8_t reg)
{
	outb(CMOS_ADDR_PORT, reg);
	return inb(CMOS_DATA_PORT);
}

static void
rtcWrite(uint8_t reg, uint8_t value)
{
	outb(CMOS_ADDR_PORT, reg);
	outb(CMOS_DATA_PORT, value);
}

#define BCD_TO_BIN(val) (((val) & 0x0F) + ((val) >> 4) * 10)
#define BIN_TO_BCD(val) ((((val) / 10) << 4) | ((val) % 10))

static bool
isLeapYear(int year)
{
	return (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
}

/*
 * Days-since-epoch civil-calendar conversion (proleptic Gregorian, matches
 * the algorithm POSIX gmtime/timegm use) -- self-contained since kernel code
 * can't call libc's mktime/timegm.
 */
static long
daysFromEpoch(int year, int month, int day)
{
	static const int cumulativeDays[12] = { 0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334 };
	long days = (year - 1970) * 365 + cumulativeDays[month - 1] + (day - 1);

	for (int y = 1970; y < year; y++) {
		if (isLeapYear(y)) days++;
	}
	if (month > 2 && isLeapYear(year)) days++;

	return days;
}

long
PDACPIPlatformExpert::getGMTTimeOfDay(void)
{
	uint8_t sec, min, hour, day, month, year, statusB;

	/* Skip an in-progress update rather than reading a torn value. */
	while (rtcRead(RTC_STATUS_A) & RTC_STATUS_A_UPDATE_IN_PROGRESS) {
		;
	}

	sec = rtcRead(RTC_SECONDS);
	min = rtcRead(RTC_MINUTES);
	hour = rtcRead(RTC_HOURS);
	day = rtcRead(RTC_DAY_OF_MONTH);
	month = rtcRead(RTC_MONTH);
	year = rtcRead(RTC_YEAR);
	statusB = rtcRead(RTC_STATUS_B);

	if (!(statusB & RTC_STATUS_B_BINARY)) {
		sec = BCD_TO_BIN(sec);
		min = BCD_TO_BIN(min);
		hour = BCD_TO_BIN(hour & 0x7F);
		day = BCD_TO_BIN(day);
		month = BCD_TO_BIN(month);
		year = BCD_TO_BIN(year);
	}
	if (!(statusB & RTC_STATUS_B_24_HOUR) && (hour & 0x80)) {
		hour = ((hour & 0x7F) % 12) + 12;
	}

	/* CMOS only stores a 2-digit year; assume 2000-2099, same as every
	 * other PC RTC driver (real Darwin's AppleRTC has the same
	 * assumption baked in for the same hardware). */
	int fullYear = 2000 + year;

	long days = daysFromEpoch(fullYear, month, day);
	return days * 86400 + hour * 3600 + min * 60 + sec;
}

void
PDACPIPlatformExpert::setGMTTimeOfDay(long secs)
{
	long days = secs / 86400;
	long remainder = secs % 86400;
	if (remainder < 0) {
		remainder += 86400;
		days--;
	}

	int hour = (int)(remainder / 3600);
	int min = (int)((remainder % 3600) / 60);
	int sec = (int)(remainder % 60);

	int year = 1970;
	for (;;) {
		int daysInYear = isLeapYear(year) ? 366 : 365;
		if (days < daysInYear) break;
		days -= daysInYear;
		year++;
	}

	static const int monthLengths[12] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
	int month = 0;
	while (true) {
		int len = monthLengths[month];
		if (month == 1 && isLeapYear(year)) len = 29;
		if (days < len) break;
		days -= len;
		month++;
	}
	int day = (int)days + 1;

	uint8_t statusB = rtcRead(RTC_STATUS_B);
	bool binary = statusB & RTC_STATUS_B_BINARY;

	uint8_t bSec = binary ? (uint8_t)sec : BIN_TO_BCD(sec);
	uint8_t bMin = binary ? (uint8_t)min : BIN_TO_BCD(min);
	uint8_t bHour = binary ? (uint8_t)hour : BIN_TO_BCD(hour);
	uint8_t bDay = binary ? (uint8_t)day : BIN_TO_BCD(day);
	uint8_t bMonth = binary ? (uint8_t)(month + 1) : BIN_TO_BCD(month + 1);
	uint8_t bYear = binary ? (uint8_t)(year - 2000) : BIN_TO_BCD(year - 2000);

	/* Halt updates while writing so we don't race the RTC's own tick. */
	rtcWrite(RTC_STATUS_B, statusB | 0x80);
	rtcWrite(RTC_SECONDS, bSec);
	rtcWrite(RTC_MINUTES, bMin);
	rtcWrite(RTC_HOURS, bHour);
	rtcWrite(RTC_DAY_OF_MONTH, bDay);
	rtcWrite(RTC_MONTH, bMonth);
	rtcWrite(RTC_YEAR, bYear);
	rtcWrite(RTC_STATUS_B, statusB);
}

#pragma mark - SMP

// XNU's x86 SMP entry point; no x86 IOKit code calls it, so a platform kext must.
extern "C" kern_return_t ml_processor_register(cpu_id_t cpu_id, uint32_t lapic_id,
                                               processor_t *processor_out,
                                               boolean_t boot_cpu, boolean_t start);

/* Local APIC id of the processor this code is running on, i.e. the BSP. */
static uint32_t pd_boot_lapic_id(void) {
	uint32_t eax, ebx, ecx, edx;
	__asm__ volatile ("cpuid"
	                  : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx)
	                  : "a"(1), "c"(0));
	return (ebx >> 24) & 0xff;
}

// Enabled processors from the MADT, boot CPU first.
unsigned PDACPIPlatformExpert::enumerateProcessorsFromMADT(void) {
	if (!fUACPIStarted) return 0;

	uacpi_table table;
	if (uacpi_unlikely_error(uacpi_table_find_by_signature(ACPI_MADT_SIGNATURE,
	                                                       &table))) {
		kprintf("PDACPIPlatform: no MADT; staying uniprocessor\n");
		return 0;
	}

	struct acpi_madt *madt = (struct acpi_madt *)table.ptr;
	uint32_t len = madt->hdr.length;
	uint32_t off = sizeof(*madt);
	uint32_t bootLapic = pd_boot_lapic_id();
	unsigned count = 0;
	bool haveBoot = false;

	while (off + sizeof(struct acpi_entry_hdr) <= len && count < kMaxCPUs) {
		struct acpi_entry_hdr *e =
		    (struct acpi_entry_hdr *)((uint8_t *)madt + off);

		/* A zero length would spin here forever on a malformed table. */
		if (e->length < sizeof(*e) || off + e->length > len) break;

		if (e->type == ACPI_MADT_ENTRY_TYPE_LAPIC &&
		    e->length >= sizeof(struct acpi_madt_lapic)) {
			struct acpi_madt_lapic *l = (struct acpi_madt_lapic *)e;

			if (l->flags & 1) {   /* enabled */
				if (l->id == bootLapic) {
					// XNU asserts the boot_cpu registration lands on cpu 0.
					fLapicIds[count++] = fLapicIds[0];
					fLapicIds[0] = l->id;
					haveBoot = true;
				} else {
					fLapicIds[count++] = l->id;
				}
			}
		}
		off += e->length;
	}

	uacpi_table_unref(&table);

	if (!haveBoot) {
		kprintf("PDACPIPlatform: MADT lists no entry for boot LAPIC %u; "
		        "staying uniprocessor\n", bootLapic);
		return 0;
	}

	kprintf("PDACPIPlatform: MADT reports %u enabled processor%s (boot LAPIC %u)\n",
	        count, count == 1 ? "" : "s", bootLapic);
	return count;
}

// Two passes, as ml_processor_register requires: register every CPU so the
// topology sort sees the full set, then start them.
void PDACPIPlatformExpert::registerProcessors(void) {
	processor_t procs[kMaxCPUs];
	AppleI386CPU *cpus[kMaxCPUs];

	// cpu_id is an IOCPU *, not an index: PE_cpu_machine_init() casts it back.
	cpus[0] = bootCPU;
	for (unsigned i = 1; i < fCPUCount; i++) {
		cpus[i] = new AppleI386CPU;
		if (cpus[i] == 0 || !cpus[i]->prepareSecondary(i)) {
			kprintf("PDACPIPlatform: no IOCPU for LAPIC %u; capping at %u CPUs\n",
			        fLapicIds[i], i);
			fCPUCount = i;
			break;
		}
	}

	for (unsigned i = 0; i < fCPUCount; i++) {
		procs[i] = NULL;
		kern_return_t kr = ml_processor_register((cpu_id_t)cpus[i],
		                                         fLapicIds[i], &procs[i],
		                                         i == 0, FALSE);
		if (kr != KERN_SUCCESS) {
			kprintf("PDACPIPlatform: registering LAPIC %u failed (0x%x)\n",
			        fLapicIds[i], kr);
			fCPUCount = i;   // do not start what was never registered
			break;
		}
	}

	for (unsigned i = 0; i < fCPUCount; i++) {
		kern_return_t kr = ml_processor_register((cpu_id_t)cpus[i],
		                                         fLapicIds[i], &procs[i],
		                                         i == 0, TRUE);
		if (kr != KERN_SUCCESS) {
			kprintf("PDACPIPlatform: starting LAPIC %u failed (0x%x)\n",
			        fLapicIds[i], kr);
		}
	}
}

#pragma mark - ACPI (uACPI)

// ~56 bytes per table; 4 KB covers about 73.
static uint8_t gEarlyTableBuffer[4096] __attribute__((aligned(sizeof(void *))));

bool PDACPIPlatformExpert::setupEarlyTables(void) {
	uacpi_status st = uacpi_setup_early_table_access(gEarlyTableBuffer,
	                                                 sizeof(gEarlyTableBuffer));
	if (uacpi_unlikely_error(st)) {
		kprintf("PDACPIPlatform: early table access failed: %s\n",
		        uacpi_status_to_string(st));
		return false;
	}
	return true;
}

// Log a table's presence and revision.
void PDACPIPlatformExpert::logTable(const char *signature) {
	uacpi_table table;

	uacpi_status st = uacpi_table_find_by_signature(signature, &table);
	if (uacpi_unlikely_error(st)) {
		kprintf("PDACPIPlatform:   %.4s absent\n", signature);
		return;
	}

	kprintf("PDACPIPlatform:   %.4s revision %u, %u bytes, OEM '%.6s'\n",
	        table.hdr->signature, table.hdr->revision, table.hdr->length,
	        table.hdr->oemid);

	uacpi_table_unref(&table);
}

void PDACPIPlatformExpert::free(void) {
	// Matching frees instances that never ran start(); their teardown must not
	// touch the glue's global state.
	if (fUACPIStarted) {
		uacpi_state_reset();
		PDACPIGlueFree();
		fUACPIStarted = false;
	}
	super::free();
}

// The one family entry point that works without a namespace.
const OSData *PDACPIPlatformExpert::getACPITableData(const char *tableName,
                                                     UInt32 tableInstance) {
	if (tableName == 0 || !fUACPIStarted) return 0;

	uacpi_table table;
	uacpi_status st = uacpi_table_find_by_signature_at(tableName, tableInstance,
	                                                   &table);
	if (uacpi_unlikely_error(st)) return 0;

	// Copy: the caller outlives the mapping.
	OSData *data = OSData::withBytes(table.ptr, table.hdr->length);
	uacpi_table_unref(&table);
	return data;
}

SInt32 PDACPIPlatformExpert::installDeviceInterruptForFixedEvent(IOService *, UInt32) {
	return -1;
}

SInt32 PDACPIPlatformExpert::installDeviceInterruptForGPE(IOService *, UInt32, void *,
                                                          IOOptionBits) {
	return -1;
}

IOReturn PDACPIPlatformExpert::acquireGlobalLock(IOService *, UInt32 *,
                                                 const mach_timespec_t *) {
	return kIOReturnUnsupported;
}

void PDACPIPlatformExpert::releaseGlobalLock(IOService *, UInt32) {
}

IOReturn PDACPIPlatformExpert::validateObject(IOACPIPlatformDevice *, const OSSymbol *) {
	return kIOReturnUnsupported;
}

IOReturn PDACPIPlatformExpert::evaluateObject(IOACPIPlatformDevice *, const OSSymbol *,
                                              OSObject **, OSObject *[], IOItemCount,
                                              IOOptionBits) {
	return kIOReturnUnsupported;
}

IOReturn PDACPIPlatformExpert::registerAddressSpaceHandler(IOACPIPlatformDevice *,
                                                           IOACPIAddressSpaceID,
                                                           IOACPIAddressSpaceHandler,
                                                           void *, IOOptionBits) {
	return kIOReturnUnsupported;
}

void PDACPIPlatformExpert::unregisterAddressSpaceHandler(IOACPIPlatformDevice *,
                                                         IOACPIAddressSpaceID,
                                                         IOACPIAddressSpaceHandler,
                                                         IOOptionBits) {
}

IOReturn PDACPIPlatformExpert::readAddressSpace(UInt64 *, IOACPIAddressSpaceID,
                                                IOACPIAddress, UInt32, UInt32,
                                                IOOptionBits) {
	return kIOReturnUnsupported;
}

IOReturn PDACPIPlatformExpert::writeAddressSpace(UInt64, IOACPIAddressSpaceID,
                                                 IOACPIAddress, UInt32, UInt32,
                                                 IOOptionBits) {
	return kIOReturnUnsupported;
}

IOReturn PDACPIPlatformExpert::setDevicePowerState(IOACPIPlatformDevice *, UInt32) {
	return kIOReturnUnsupported;
}

IOReturn PDACPIPlatformExpert::getDevicePowerState(IOACPIPlatformDevice *, UInt32 *) {
	return kIOReturnUnsupported;
}

IOReturn PDACPIPlatformExpert::setDeviceWakeEnable(IOACPIPlatformDevice *, bool) {
	return kIOReturnUnsupported;
}

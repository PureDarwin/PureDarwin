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
#include <IOKit/platform/ApplePlatformExpert.h>
#include <libkern/c++/OSContainers.h>
#include <libkern/c++/OSUnserialize.h>
#include <pexpert/i386/boot.h>

extern "C" {
#include <i386/cpuid.h>
#include <pexpert/i386/protos.h>
}

#include "../AppleAPIC/PICShared.h"
#include "AppleI386PlatformExpert.h"

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

class AppleI386PlatformExpertGlobals {
public:
	bool isValid;
	AppleI386PlatformExpertGlobals();
	~AppleI386PlatformExpertGlobals();
};

static AppleI386PlatformExpertGlobals AppleI386PlatformExpertGlobals;
AppleI386PlatformExpertGlobals::AppleI386PlatformExpertGlobals() {
	ResourceLock = IOLockAlloc();
	bzero(IRQ, sizeof(IRQ));
}

AppleI386PlatformExpertGlobals::~AppleI386PlatformExpertGlobals() {
	if (ResourceLock) IOLockFree(ResourceLock);
}

#pragma mark -

#define super IOPlatformExpert

OSDefineMetaClassAndStructors(AppleI386PlatformExpert, IOPlatformExpert);

IOService *AppleI386PlatformExpert::probe(IOService *provider, SInt32 *score) {
	if (score != 0) *score = 10000;
	return this;
}

bool AppleI386PlatformExpert::init(OSDictionary *properties) {\
	if (!super::init(properties)) return false;

	OSString *name = (OSString *)getProperty("InterruptControllerName");
	if (name == 0) name = OSString::withCStringNoCopy("AppleI386CPUInterruptController");
	_interruptControllerName = OSSymbol::withString(name);

	return true;
}

bool AppleI386PlatformExpert::start(IOService *provider) {
	setBootROMType(kBootROMTypeNewWorld);

	bool superOK = super::start(provider);
	if (!superOK) return false;
	PE_halt_restart = handlePEHaltRestart;
	registerService();

	// Hack: Initialize AppleI386CPU ourself because no one else will.
	bootCPU = new AppleI386CPU;
	if (bootCPU == 0) return false;

	bootCPU->init();
	bootCPU->attach(0);
	if (!bootCPU->startCommon()) return false;

	return true;
}

bool AppleI386PlatformExpert::configure(IOService *provider) {
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
			nub->registerService();
			kprintf(">>>   registered nub '%s'\n", nub->getName());
			nub->release();
		}
	}

	return true;
}

bool AppleI386PlatformExpert::matchNubWithPropertyTable(IOService *nub, OSDictionary *table) {
	OSString *nameProp;
	OSString *match;

	if ((nameProp = (OSString *)nub->getProperty(gIONameKey)) == 0) return false;
	if ((match = (OSString *)table->getObject(gIONameMatchKey)) == 0) return false;

	return match->isEqualTo(nameProp);
}

IOService *AppleI386PlatformExpert::createNub(OSDictionary *from) {
	IOService *nub;

	nub = super::createNub(from);
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

void AppleI386PlatformExpert::setupPIC(IOService *nub) {
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

void AppleI386PlatformExpert::setupBIOS(IOService *nub) {
	// TODO: Implement this function.
	// This function is dependent upon being able to retrieve the
	// PCI bus data. While the booter does collect some PCI data,
	// but it does not include the data needed here.
}

bool AppleI386PlatformExpert::getMachineName(char *name, int maxLength) {
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

bool AppleI386PlatformExpert::getModelName(char *name, int maxLengh) {
	i386_cpu_info_t *cpuid_cpu_info = cpuid_info();

	if (cpuid_cpu_info->cpuid_brand_string[0] != '\0') {
		strncpy(name, cpuid_cpu_info->cpuid_brand_string, maxLengh);
	} else {
		strncpy(name, cpuid_cpu_info->cpuid_model_string, maxLengh);
	}

	return true;
}

int AppleI386PlatformExpert::handlePEHaltRestart(unsigned int type) {
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
		default:
			ret = -1;
			break;
	}

	return ret;
}

bool AppleI386PlatformExpert::setNubInterruptVectors(IOService *nub, const UInt32 *vectors, UInt32 vectorCount) {
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

bool AppleI386PlatformExpert::setNubInterruptVector(IOService *nub, UInt32 vector) {
	return setNubInterruptVectors(nub, &vector, 1);
}

IOReturn AppleI386PlatformExpert::callPlatformFunction(const OSSymbol *functionName, bool waitForFunction, void *param1, void *param2, void *param3, void *param4) {
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
		kprintf("AppleI386PlatformExpert: MSI msg vector=0x%x addr=0x%08x data=0x%x\n",
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

bool AppleI386PlatformExpert::reserveSystemInterrupt(IOService *client, UInt32 vectorNumber, bool exclusive) {
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

void AppleI386PlatformExpert::releaseSystemInterrupt(IOService *client, UInt32 vectorNumber, bool exclusive) {
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
AppleI386PlatformExpert::getGMTTimeOfDay(void)
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
AppleI386PlatformExpert::setGMTTimeOfDay(long secs)
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

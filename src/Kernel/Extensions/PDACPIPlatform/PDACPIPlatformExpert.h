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
 *
 * Adapted from AppleI386PlatformExpert into PDACPIPlatform: the same x86
 * platform duties, but rooted on IOACPIPlatformExpert so one object is both
 * the platform expert and IOACPIFamily's provider, as AppleACPIPlatform is.
 */

#ifndef _PDACPIPLATFORMEXPERT_H
#define _PDACPIPLATFORMEXPERT_H

#include <IOKit/acpi/IOACPIPlatformExpert.h>
#include "AppleI386CPU.h"

bool PDACPIGlueInit(void);
void PDACPIGlueFree(void);

class PDACPIPlatformExpert : public IOACPIPlatformExpert {
	OSDeclareDefaultStructors(PDACPIPlatformExpert)

private:
	const OSSymbol *_interruptControllerName;
	AppleI386CPU *bootCPU;
	bool fUACPIStarted;

	void setupPIC(IOService *nub);
	void setupBIOS(IOService *nub);

	// uACPI's barebones stage: tables readable, no namespace, no AML.
	bool setupEarlyTables(void);
	void logTable(const char *signature);

	// Enabled processors' LAPIC ids, boot CPU first (XNU assigns it slot 0).
	enum { kMaxCPUs = 64 };
	uint32_t fLapicIds[kMaxCPUs];
	unsigned fCPUCount;

	unsigned enumerateProcessorsFromMADT(void);
	void     registerProcessors(void);

	static int handlePEHaltRestart(unsigned int type);

public:
	virtual const char *deleteList(void) APPLE_KEXT_OVERRIDE;
	virtual const char *excludeList(void) APPLE_KEXT_OVERRIDE;
	virtual bool init(OSDictionary *properties) APPLE_KEXT_OVERRIDE;
	virtual IOService *probe(IOService *provider, SInt32 *score) APPLE_KEXT_OVERRIDE;
	virtual bool start(IOService *provider) APPLE_KEXT_OVERRIDE;
	void publishPlatformUUIDFromDeviceTree(void);
	void cacheACPIPowerOffFromDeviceTree(void);

	/* PM1a/PM1b control ports for ACPI soft-off, found by walking the FADT at
	 * start(). PEHaltRestart can be reached from panic context, where mapping
	 * memory and taking IORegistry locks is not safe, so the lookup happens
	 * once up front and the halt path only does an outw(). */
	static UInt16 sPM1aControlPort;
	static UInt16 sPM1bControlPort;
	static UInt8  sS5SleepTypeA;
	static UInt8  sS5SleepTypeB;
	static bool   sACPIPowerOffReady;
	virtual bool configure(IOService *provider) APPLE_KEXT_OVERRIDE;
	virtual bool matchNubWithPropertyTable(IOService *nub, OSDictionary *table);
	virtual IOService *createNub(OSDictionary *from) APPLE_KEXT_OVERRIDE;
	virtual bool reserveSystemInterrupt(IOService *client, UInt32 vectorNumber, bool exclusive);
	virtual void releaseSystemInterrupt(IOService *client, UInt32 vectorNumber, bool exclusive);
	virtual bool setNubInterruptVectors(IOService *nub, const UInt32 vectors[], UInt32 vectorCount);
	virtual bool setNubInterruptVector(IOService *nub, UInt32 vector);
	virtual IOReturn callPlatformFunction(const OSSymbol *functionName, bool waitForFunction, void *param1, void *param2, void *param3, void *param4) APPLE_KEXT_OVERRIDE;
	virtual bool getModelName(char *name, int maxLengh) APPLE_KEXT_OVERRIDE;
	virtual bool getMachineName(char *name, int maxLength) APPLE_KEXT_OVERRIDE;
	virtual long getGMTTimeOfDay(void) APPLE_KEXT_OVERRIDE;
	virtual void setGMTTimeOfDay(long secs) APPLE_KEXT_OVERRIDE;
	virtual void free(void) APPLE_KEXT_OVERRIDE;

protected:
	// IOACPIPlatformExpert's pure virtuals; only getACPITableData is real so far.
	virtual const OSData * getACPITableData(const char *tableName,
	                                        UInt32 tableInstance) APPLE_KEXT_OVERRIDE;

	virtual SInt32 installDeviceInterruptForFixedEvent(IOService *device,
	                                                   UInt32 fixedEvent) APPLE_KEXT_OVERRIDE;
	virtual SInt32 installDeviceInterruptForGPE(IOService *device, UInt32 gpeNumber,
	                                            void *gpeBlockDevice,
	                                            IOOptionBits options) APPLE_KEXT_OVERRIDE;

	virtual IOReturn acquireGlobalLock(IOService *client, UInt32 *lockToken,
	                                   const mach_timespec_t *timeout) APPLE_KEXT_OVERRIDE;
	virtual void releaseGlobalLock(IOService *client, UInt32 lockToken) APPLE_KEXT_OVERRIDE;

	virtual IOReturn validateObject(IOACPIPlatformDevice *device,
	                                const OSSymbol *objectName) APPLE_KEXT_OVERRIDE;
	virtual IOReturn evaluateObject(IOACPIPlatformDevice *device,
	                                const OSSymbol *objectName, OSObject **result,
	                                OSObject *params[], IOItemCount paramCount,
	                                IOOptionBits options) APPLE_KEXT_OVERRIDE;

	virtual IOReturn registerAddressSpaceHandler(IOACPIPlatformDevice *device,
	                                             IOACPIAddressSpaceID spaceID,
	                                             IOACPIAddressSpaceHandler handler,
	                                             void *context,
	                                             IOOptionBits options) APPLE_KEXT_OVERRIDE;
	virtual void unregisterAddressSpaceHandler(IOACPIPlatformDevice *device,
	                                           IOACPIAddressSpaceID spaceID,
	                                           IOACPIAddressSpaceHandler handler,
	                                           IOOptionBits options) APPLE_KEXT_OVERRIDE;

	virtual IOReturn readAddressSpace(UInt64 *value, IOACPIAddressSpaceID spaceID,
	                                  IOACPIAddress address, UInt32 bitWidth,
	                                  UInt32 bitOffset, IOOptionBits options) APPLE_KEXT_OVERRIDE;
	virtual IOReturn writeAddressSpace(UInt64 value, IOACPIAddressSpaceID spaceID,
	                                   IOACPIAddress address, UInt32 bitWidth,
	                                   UInt32 bitOffset, IOOptionBits options) APPLE_KEXT_OVERRIDE;

	virtual IOReturn setDevicePowerState(IOACPIPlatformDevice *device,
	                                     UInt32 powerState) APPLE_KEXT_OVERRIDE;
	virtual IOReturn getDevicePowerState(IOACPIPlatformDevice *device,
	                                     UInt32 *powerState) APPLE_KEXT_OVERRIDE;
	virtual IOReturn setDeviceWakeEnable(IOACPIPlatformDevice *device,
	                                     bool enable) APPLE_KEXT_OVERRIDE;
};

#endif /* ! _PDACPIPLATFORMEXPERT_H */

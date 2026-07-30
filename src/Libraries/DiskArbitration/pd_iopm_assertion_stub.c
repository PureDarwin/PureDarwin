/*
 * IOPMAssertion: the IOKit power-management API for telling powerd "do not let
 * the system idle-sleep while I am doing this". The real implementation is
 * IOKitUser's pwr_mgt.subproj/IOPMAssertions.c, which is a client of powerd -
 * and PureDarwin has no powerd, no idle sleep, and no sleep at all.
 */

#include <IOKit/IOReturn.h>
#include <IOKit/pwr_mgt/IOPMLib.h>

IOReturn
IOPMAssertionCreateWithDescription(CFStringRef assertionType,
				   CFStringRef name, CFStringRef details,
				   CFStringRef humanReadableReason,
				   CFStringRef localizationBundlePath,
				   CFTimeInterval timeout,
				   CFStringRef timeoutAction,
				   IOPMAssertionID *assertionID)
{
	(void)assertionType;
	(void)name;
	(void)details;
	(void)humanReadableReason;
	(void)localizationBundlePath;
	(void)timeout;
	(void)timeoutAction;
	if (assertionID != NULL) {
		*assertionID = 0;
	}
	return kIOReturnSuccess;
}

IOReturn
IOPMAssertionRelease(IOPMAssertionID assertionID)
{
	(void)assertionID;
	return kIOReturnSuccess;
}

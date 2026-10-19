/*
 * IOPMLib - the client side of system power management.
 *
 * Apple implements this in IOKitUser as an IPC client of powerd: IOPMConnection
 * is a registration for sleep/wake notifications, and IOAllowPowerChange is how
 * a client acknowledges one so the transition can proceed. PureDarwin has no
 * powerd and no system sleep, so there is nothing to register with and nothing
 * that could ever acknowledge a transition.
 *
 * These report "no power management" rather than pretending to succeed. A caller
 * that checks the return - IPConfiguration does - simply does not install its
 * sleep/wake handling and keeps running on the normal path, which is correct on
 * a system that never sleeps. Silently returning success would leave callers
 * waiting for wake events that can never arrive.
 *
 * Same shape as IOPowerSources.c beside it, for the same reason.
 */

#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/IOReturn.h>
#include <IOKit/pwr_mgt/IOPMLib.h>
/* IOPMConnection and the sleep/wake notification types are private: Apple ships
 * them in IOPMLibPrivate.h, not in the SDK. */
#include <IOKit/pwr_mgt/IOPMLibPrivate.h>

IOReturn
IOPMConnectionCreate(CFStringRef myName, IOPMSystemPowerStateCapabilities interests,
                     IOPMConnection *newConnection)
{
	(void)myName;
	(void)interests;
	if (newConnection != NULL) {
		*newConnection = NULL;
	}
	return kIOReturnUnsupported;
}

IOReturn
IOPMConnectionSetNotification(IOPMConnection myConnection, void *param,
                              IOPMEventHandlerType handler)
{
	(void)myConnection;
	(void)param;
	(void)handler;
	return kIOReturnUnsupported;
}

IOReturn
IOPMConnectionScheduleWithRunLoop(IOPMConnection myConnection, CFRunLoopRef theRunLoop,
                                  CFStringRef runLoopMode)
{
	(void)myConnection;
	(void)theRunLoop;
	(void)runLoopMode;
	return kIOReturnUnsupported;
}

IOReturn
IOPMConnectionUnscheduleFromRunLoop(IOPMConnection myConnection, CFRunLoopRef theRunLoop,
                                    CFStringRef runLoopMode)
{
	(void)myConnection;
	(void)theRunLoop;
	(void)runLoopMode;
	return kIOReturnUnsupported;
}

IOReturn
IOPMConnectionRelease(IOPMConnection connection)
{
	(void)connection;
	return kIOReturnUnsupported;
}

/*
 * Acknowledging an event that was never delivered. Callers ignore the result of
 * this one - there is nothing sensible to do if an acknowledgement fails - so it
 * only has to be harmless.
 */
IOReturn
IOPMConnectionAcknowledgeEvent(IOPMConnection connect, IOPMConnectionMessageToken token)
{
	(void)connect;
	(void)token;
	return kIOReturnUnsupported;
}

IOReturn
IOPMConnectionAcknowledgeEventWithOptions(IOPMConnection connect,
                                          IOPMConnectionMessageToken token,
                                          CFDictionaryRef options)
{
	(void)connect;
	(void)token;
	(void)options;
	return kIOReturnUnsupported;
}

IOReturn
IOAllowPowerChange(io_connect_t kernelPort, intptr_t notificationID)
{
	(void)kernelPort;
	(void)notificationID;
	return kIOReturnUnsupported;
}

IOReturn
IOCancelPowerChange(io_connect_t kernelPort, intptr_t notificationID)
{
	(void)kernelPort;
	(void)notificationID;
	return kIOReturnUnsupported;
}

IOReturn
IOPMCancelScheduledPowerEvent(CFDateRef time_to_wake, CFStringRef my_id, CFStringRef type)
{
	(void)time_to_wake;
	(void)my_id;
	(void)type;
	return kIOReturnUnsupported;
}

IOReturn
IOPMSchedulePowerEvent(CFDateRef time_to_wake, CFStringRef my_id, CFStringRef type)
{
	(void)time_to_wake;
	(void)my_id;
	(void)type;
	return kIOReturnUnsupported;
}

/*
 * The older IORegisterForSystemPower/IOPMCopyScheduledPowerEvents interfaces,
 * used alongside IOPMConnection. Same reasoning: no powerd, no sleep.
 */
io_connect_t
IORegisterForSystemPower(void *refcon, IONotificationPortRef *thePortRef,
                         IOServiceInterestCallback callback, io_object_t *notifier)
{
	(void)refcon;
	(void)callback;
	if (thePortRef != NULL) {
		*thePortRef = NULL;
	}
	if (notifier != NULL) {
		*notifier = IO_OBJECT_NULL;
	}
	return MACH_PORT_NULL;
}

CFArrayRef
IOPMCopyScheduledPowerEvents(void)
{
	/* An empty array rather than NULL: callers iterate the result, and
	 * IPConfiguration's CleanupWakeEvents does so without a NULL check. */
	return CFArrayCreate(kCFAllocatorDefault, NULL, 0, &kCFTypeArrayCallBacks);
}

IOReturn
IOPMRequestSysWake(CFDictionaryRef request)
{
	(void)request;
	return kIOReturnUnsupported;
}

// PureDarwin never sleeps, so every assertion is honoured by construction
// they are tracked so that releasing an unknown ID still fails as powerd's would
#include <pthread.h>
#include <stdlib.h>
#include <mach/mach_time.h>

static pthread_mutex_t assertionLock = PTHREAD_MUTEX_INITIALIZER;
static CFMutableDictionaryRef assertions;
static IOPMAssertionID nextAssertionID = 1;

IOReturn
IOPMAssertionCreateWithName(CFStringRef assertionType, IOPMAssertionLevel assertionLevel,
                            CFStringRef assertionName, IOPMAssertionID *assertionID)
{
	if (assertionType == NULL || assertionName == NULL || assertionID == NULL ||
	    (assertionLevel != kIOPMAssertionLevelOn && assertionLevel != kIOPMAssertionLevelOff)) {
		return kIOReturnBadArgument;
	}
	CFStringRef keys[] = { kIOPMAssertionTypeKey, kIOPMAssertionNameKey };
	CFTypeRef values[] = { assertionType, assertionName };
	CFDictionaryRef props = CFDictionaryCreate(kCFAllocatorDefault, (const void **)keys, values, 2,
	    &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);

	pthread_mutex_lock(&assertionLock);
	if (assertions == NULL) {
		assertions = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, NULL, &kCFTypeDictionaryValueCallBacks);
	}
	*assertionID = nextAssertionID++;
	CFDictionarySetValue(assertions, (const void *)(uintptr_t)*assertionID, props);
	pthread_mutex_unlock(&assertionLock);
	CFRelease(props);
	return kIOReturnSuccess;
}

IOReturn
IOPMAssertionCreateWithDescription(CFStringRef assertionType, CFStringRef name, CFStringRef details,
                                   CFStringRef humanReadableReason, CFStringRef localizationBundlePath,
                                   CFTimeInterval timeout, CFStringRef timeoutAction, IOPMAssertionID *assertionID)
{
	(void)details;
	(void)humanReadableReason;
	(void)localizationBundlePath;
	(void)timeout;
	(void)timeoutAction;
	return IOPMAssertionCreateWithName(assertionType, kIOPMAssertionLevelOn, name, assertionID);
}

IOReturn
IOPMAssertionRelease(IOPMAssertionID assertionID)
{
	Boolean found = false;

	pthread_mutex_lock(&assertionLock);
	if (assertions != NULL && CFDictionaryContainsKey(assertions, (const void *)(uintptr_t)assertionID)) {
		CFDictionaryRemoveValue(assertions, (const void *)(uintptr_t)assertionID);
		found = true;
	}
	pthread_mutex_unlock(&assertionLock);
	return found ? kIOReturnSuccess : kIOReturnNotFound;
}

// user activity is an assertion that also resets the idle time, through the same notify state the
// compositor publishes input times to
extern uint32_t notify_register_check(const char *name, int *out_token);
extern uint32_t notify_set_state(int token, uint64_t state);

IOReturn
IOPMAssertionDeclareUserActivity(CFStringRef AssertionName, IOPMUserActiveType userType, IOPMAssertionID *AssertionID)
{
	static int token = -1;
	static mach_timebase_info_data_t timebase;

	(void)userType;
	if (AssertionName == NULL || AssertionID == NULL) {
		return kIOReturnBadArgument;
	}
	if (timebase.denom == 0) {
		mach_timebase_info(&timebase);
	}
	if (token >= 0 || notify_register_check("org.puredarwin.hid.lastinput", &token) == 0) {
		notify_set_state(token, mach_absolute_time() * timebase.numer / timebase.denom);
	}
	if (*AssertionID != kIOPMNullAssertionID) {
		pthread_mutex_lock(&assertionLock);
		Boolean held = assertions != NULL && CFDictionaryContainsKey(assertions, (const void *)(uintptr_t)*AssertionID);
		pthread_mutex_unlock(&assertionLock);
		if (held) {
			return kIOReturnSuccess;
		}
	}
	return IOPMAssertionCreateWithName(kIOPMAssertPreventUserIdleDisplaySleep, kIOPMAssertionLevelOn,
	                                   AssertionName, AssertionID);
}

// the level of each assertion type, on while any assertion of that type is held
IOReturn
IOPMCopyAssertionsStatus(CFDictionaryRef *AssertionsStatus)
{
	if (AssertionsStatus == NULL) {
		return kIOReturnBadArgument;
	}

	CFMutableDictionaryRef status = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
	    &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
	int on = kIOPMAssertionLevelOn;
	CFNumberRef onNumber = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &on);

	pthread_mutex_lock(&assertionLock);
	if (assertions != NULL) {
		CFIndex count = CFDictionaryGetCount(assertions);
		const void **values = malloc(sizeof(void *) * (count ? count : 1));

		CFDictionaryGetKeysAndValues(assertions, NULL, values);
		for (CFIndex i = 0; i < count; i++) {
			CFStringRef type = CFDictionaryGetValue((CFDictionaryRef)values[i], kIOPMAssertionTypeKey);
			if (type != NULL) {
				CFDictionarySetValue(status, type, onNumber);
			}
		}
		free(values);
	}
	pthread_mutex_unlock(&assertionLock);
	CFRelease(onNumber);
	*AssertionsStatus = status;
	return kIOReturnSuccess;
}

// IORegisterForSystemPower never hands out a notifier, so there is nothing to undo
IOReturn
IODeregisterForSystemPower(io_object_t *notifier)
{
	if (notifier == NULL || *notifier == IO_OBJECT_NULL) {
		return kIOReturnBadArgument;
	}
	IOObjectRelease(*notifier);
	*notifier = IO_OBJECT_NULL;
	return kIOReturnSuccess;
}

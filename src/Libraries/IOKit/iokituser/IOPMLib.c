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

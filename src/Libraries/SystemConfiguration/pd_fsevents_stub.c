#include <CoreFoundation/CoreFoundation.h>
#include <CoreServices/CoreServices.h>

FSEventStreamRef
FSEventStreamCreate(CFAllocatorRef allocator, FSEventStreamCallback callback,
		    FSEventStreamContext *context, CFArrayRef pathsToWatch,
		    FSEventStreamEventId sinceWhen, CFTimeInterval latency,
		    FSEventStreamCreateFlags flags)
{
	(void)callback; (void)context; (void)pathsToWatch;
	(void)sinceWhen; (void)latency; (void)flags;

	return (FSEventStreamRef)CFDataCreate(allocator, (const UInt8 *)"", 0);
}

void
FSEventStreamScheduleWithRunLoop(FSEventStreamRef streamRef,
				 CFRunLoopRef runLoop, CFStringRef runLoopMode)
{
	(void)streamRef; (void)runLoop; (void)runLoopMode;
}

Boolean
FSEventStreamStart(FSEventStreamRef streamRef)
{
	(void)streamRef;
	return TRUE;
}

void
FSEventStreamStop(FSEventStreamRef streamRef)
{
	(void)streamRef;
}

void
FSEventStreamInvalidate(FSEventStreamRef streamRef)
{
	(void)streamRef;
}

void
FSEventStreamRelease(FSEventStreamRef streamRef)
{
	if (streamRef != NULL) {
		CFRelease((CFTypeRef)streamRef);
	}
}

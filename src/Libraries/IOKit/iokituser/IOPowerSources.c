/*
 * IOPowerSources - power-source enumeration.
 *
 * Apple implements this in IOKitUser on top of IOPMLib and the powerd
 * notification machinery, neither of which exists here yet.
 */

#include <CoreFoundation/CoreFoundation.h>

CFTypeRef IOPSCopyPowerSourcesInfo(void);
CFArrayRef IOPSCopyPowerSourcesList(CFTypeRef blob);
CFDictionaryRef IOPSGetPowerSourceDescription(CFTypeRef blob, CFTypeRef ps);

/*
 * The blob is an opaque token that only IOPSCopyPowerSourcesList and
 * IOPSGetPowerSourceDescription interpret. An empty dictionary is a valid
 * CFTypeRef for callers to CFRelease, which is all they do with it.
 */
CFTypeRef
IOPSCopyPowerSourcesInfo(void)
{
	return CFDictionaryCreate(kCFAllocatorDefault, NULL, NULL, 0,
	    &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
}

CFArrayRef
IOPSCopyPowerSourcesList(CFTypeRef blob)
{
	(void)blob;
	return CFArrayCreate(kCFAllocatorDefault, NULL, 0,
	    &kCFTypeArrayCallBacks);
}

CFDictionaryRef
IOPSGetPowerSourceDescription(CFTypeRef blob, CFTypeRef ps)
{
	(void)blob;
	(void)ps;
	/* Not a Copy/Create function: the returned dictionary would be owned by
	 * the blob, so there is nothing to hand back for an empty list. */
	return NULL;
}

// power-source change notifications arrive on the notify(3) name powerd posts
// nothing posts it on PureDarwin yet, so the source is real but stays quiet
#include <mach/mach.h>
#include <stdlib.h>

// libnotify from libSystem, its header is not on this library's include path
#define NOTIFY_STATUS_OK 0
uint32_t notify_register_mach_port(const char *name, mach_port_t *notify_port, int flags, int *out_token);
uint32_t notify_cancel(int token);

typedef void (*IOPowerSourceCallbackType)(void *context);

#define kIOPSNotifyAnyPowerSource "com.apple.system.powersources"

struct psNotification {
	IOPowerSourceCallbackType callback;
	void *context;
	int token;
};

static void
psCallout(CFMachPortRef port, void *msg, CFIndex size, void *info)
{
	(void)port;
	(void)msg;
	(void)size;
	struct psNotification *n = info;
	n->callback(n->context);
}

static void
psRelease(const void *info)
{
	struct psNotification *n = (struct psNotification *)info;
	notify_cancel(n->token);
	free(n);
}

CFRunLoopSourceRef IOPSNotificationCreateRunLoopSource(IOPowerSourceCallbackType callback, void *context);

CFRunLoopSourceRef
IOPSNotificationCreateRunLoopSource(IOPowerSourceCallbackType callback, void *context)
{
	mach_port_t port = MACH_PORT_NULL;

	if (callback == NULL) {
		return NULL;
	}
	struct psNotification *n = calloc(1, sizeof(*n));
	if (n == NULL) {
		return NULL;
	}
	n->callback = callback;
	n->context = context;
	if (notify_register_mach_port(kIOPSNotifyAnyPowerSource, &port, 0, &n->token) != NOTIFY_STATUS_OK) {
		free(n);
		return NULL;
	}
	CFMachPortContext ctx = { 0, n, NULL, psRelease, NULL };
	Boolean shouldFree = false;
	CFMachPortRef mp = CFMachPortCreateWithPort(kCFAllocatorDefault, port, psCallout, &ctx, &shouldFree);
	if (mp == NULL) {
		psRelease(n);
		return NULL;
	}
	CFRunLoopSourceRef source = CFMachPortCreateRunLoopSource(kCFAllocatorDefault, mp, 0);
	CFRelease(mp);
	return source;
}

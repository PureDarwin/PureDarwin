/*
 * CoreServices, and the FSEvents API inside it, are not open source.
 *
 * IPMonitor's dns-configuration.c uses FSEvents in exactly one function,
 * dns_configuration_monitor(), to watch /etc/resolver for added or removed
 * per-domain resolver files. Everything else in that file - the DNS
 * configuration itself, which is what feeds dnsinfo - does not touch it.
 *
 * The stream created here is real enough to be created, scheduled and started
 * without error, but it never fires: there is nothing behind it. The observable
 * consequence is specific and worth stating plainly - changes to /etc/resolver
 * are not noticed while configd is running. The DNS configuration is still
 * computed correctly whenever anything else prompts a rebuild.
 *
 * xnu does have kernel fsevents (bsd/vfs/vfs_fsevents.c), so a genuine
 * FSEventStream over /dev/fsevents is buildable later; this is the placeholder
 * that keeps IPMonitor whole until then.
 */

#ifndef _PUREDARWIN_CORESERVICES_H_
#define _PUREDARWIN_CORESERVICES_H_

#include <CoreFoundation/CoreFoundation.h>

typedef struct __FSEventStream *FSEventStreamRef;
typedef const struct __FSEventStream *ConstFSEventStreamRef;

typedef UInt32 FSEventStreamCreateFlags;
typedef UInt32 FSEventStreamEventFlags;
typedef UInt64 FSEventStreamEventId;

enum {
	kFSEventStreamCreateFlagNone         = 0x00000000,
	kFSEventStreamCreateFlagUseCFTypes   = 0x00000001,
	kFSEventStreamCreateFlagNoDefer      = 0x00000002,
	kFSEventStreamCreateFlagWatchRoot    = 0x00000004,
	kFSEventStreamCreateFlagIgnoreSelf   = 0x00000008,
	kFSEventStreamCreateFlagFileEvents   = 0x00000010,
};

enum {
	kFSEventStreamEventFlagNone          = 0x00000000,
	kFSEventStreamEventFlagMustScanSubDirs = 0x00000001,
	kFSEventStreamEventFlagRootChanged   = 0x00000020,
	kFSEventStreamEventFlagItemCreated   = 0x00000100,
	kFSEventStreamEventFlagItemRemoved   = 0x00000200,
	kFSEventStreamEventFlagItemRenamed   = 0x00000800,
	kFSEventStreamEventFlagItemModified  = 0x00001000,
};

#define kFSEventStreamEventIdSinceNow  0xFFFFFFFFFFFFFFFFULL

typedef struct {
	CFIndex   version;
	void     *info;
	CFAllocatorRetainCallBack   retain;
	CFAllocatorReleaseCallBack  release;
	CFAllocatorCopyDescriptionCallBack copyDescription;
} FSEventStreamContext;

typedef void (*FSEventStreamCallback)(ConstFSEventStreamRef streamRef,
				     void *clientCallBackInfo,
				     size_t numEvents,
				     void *eventPaths,
				     const FSEventStreamEventFlags eventFlags[],
				     const FSEventStreamEventId eventIds[]);

FSEventStreamRef FSEventStreamCreate(CFAllocatorRef allocator,
				     FSEventStreamCallback callback,
				     FSEventStreamContext *context,
				     CFArrayRef pathsToWatch,
				     FSEventStreamEventId sinceWhen,
				     CFTimeInterval latency,
				     FSEventStreamCreateFlags flags);

void    FSEventStreamScheduleWithRunLoop(FSEventStreamRef streamRef,
					 CFRunLoopRef runLoop,
					 CFStringRef runLoopMode);
Boolean FSEventStreamStart(FSEventStreamRef streamRef);
void    FSEventStreamStop(FSEventStreamRef streamRef);
void    FSEventStreamInvalidate(FSEventStreamRef streamRef);
void    FSEventStreamRelease(FSEventStreamRef streamRef);

#endif /* _PUREDARWIN_CORESERVICES_H_ */

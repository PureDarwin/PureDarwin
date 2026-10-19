// FSEvents.h - the file system event stream API of CoreServices, see src/Libraries/CoreServices/FSEvents.c
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause

#ifndef _PD_FSEVENTS_H
#define _PD_FSEVENTS_H

#include <CoreFoundation/CoreFoundation.h>
#include <dispatch/dispatch.h>
#include <sys/types.h>
#include <uuid/uuid.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef UInt32 FSEventStreamCreateFlags;

enum {
	kFSEventStreamCreateFlagNone            = 0x00000000,
	kFSEventStreamCreateFlagUseCFTypes      = 0x00000001,
	kFSEventStreamCreateFlagNoDefer         = 0x00000002,
	kFSEventStreamCreateFlagWatchRoot       = 0x00000004,
	kFSEventStreamCreateFlagIgnoreSelf      = 0x00000008,
	kFSEventStreamCreateFlagFileEvents      = 0x00000010,
	kFSEventStreamCreateFlagMarkSelf        = 0x00000020,
	kFSEventStreamCreateFlagUseExtendedData = 0x00000040,
	kFSEventStreamCreateFlagFullHistory     = 0x00000080,
};

typedef UInt32 FSEventStreamEventFlags;

enum {
	kFSEventStreamEventFlagNone               = 0x00000000,
	kFSEventStreamEventFlagMustScanSubDirs    = 0x00000001,
	kFSEventStreamEventFlagUserDropped        = 0x00000002,
	kFSEventStreamEventFlagKernelDropped      = 0x00000004,
	kFSEventStreamEventFlagEventIdsWrapped    = 0x00000008,
	kFSEventStreamEventFlagHistoryDone        = 0x00000010,
	kFSEventStreamEventFlagRootChanged        = 0x00000020,
	kFSEventStreamEventFlagMount              = 0x00000040,
	kFSEventStreamEventFlagUnmount            = 0x00000080,
	kFSEventStreamEventFlagItemCreated        = 0x00000100,
	kFSEventStreamEventFlagItemRemoved        = 0x00000200,
	kFSEventStreamEventFlagItemInodeMetaMod   = 0x00000400,
	kFSEventStreamEventFlagItemRenamed        = 0x00000800,
	kFSEventStreamEventFlagItemModified       = 0x00001000,
	kFSEventStreamEventFlagItemFinderInfoMod  = 0x00002000,
	kFSEventStreamEventFlagItemChangeOwner    = 0x00004000,
	kFSEventStreamEventFlagItemXattrMod       = 0x00008000,
	kFSEventStreamEventFlagItemIsFile         = 0x00010000,
	kFSEventStreamEventFlagItemIsDir          = 0x00020000,
	kFSEventStreamEventFlagItemIsSymlink      = 0x00040000,
	kFSEventStreamEventFlagOwnEvent           = 0x00080000,
	kFSEventStreamEventFlagItemIsHardlink     = 0x00100000,
	kFSEventStreamEventFlagItemIsLastHardlink = 0x00200000,
	kFSEventStreamEventFlagItemCloned         = 0x00400000,
};

typedef UInt64 FSEventStreamEventId;

enum {
	kFSEventStreamEventIdSinceNow = 0xFFFFFFFFFFFFFFFFULL
};

#define kFSEventStreamEventExtendedDataPathKey CFSTR("path")
#define kFSEventStreamEventExtendedFileIDKey   CFSTR("fileID")

typedef struct __FSEventStream *FSEventStreamRef;
typedef const struct __FSEventStream *ConstFSEventStreamRef;

typedef struct FSEventStreamContext {
	CFIndex version;
	void *info;
	CFAllocatorRetainCallBack retain;
	CFAllocatorReleaseCallBack release;
	CFAllocatorCopyDescriptionCallBack copyDescription;
} FSEventStreamContext;

typedef void (*FSEventStreamCallback)(ConstFSEventStreamRef streamRef, void *clientCallBackInfo,
    size_t numEvents, void *eventPaths, const FSEventStreamEventFlags eventFlags[],
    const FSEventStreamEventId eventIds[]);

extern FSEventStreamRef FSEventStreamCreate(CFAllocatorRef allocator, FSEventStreamCallback callback,
    FSEventStreamContext *context, CFArrayRef pathsToWatch, FSEventStreamEventId sinceWhen,
    CFTimeInterval latency, FSEventStreamCreateFlags flags);
extern FSEventStreamRef FSEventStreamCreateRelativeToDevice(CFAllocatorRef allocator,
    FSEventStreamCallback callback, FSEventStreamContext *context, dev_t deviceToWatch,
    CFArrayRef pathsToWatchRelativeToDevice, FSEventStreamEventId sinceWhen,
    CFTimeInterval latency, FSEventStreamCreateFlags flags);

extern FSEventStreamEventId FSEventStreamGetLatestEventId(ConstFSEventStreamRef streamRef);
extern dev_t FSEventStreamGetDeviceBeingWatched(ConstFSEventStreamRef streamRef);
extern CFArrayRef FSEventStreamCopyPathsBeingWatched(ConstFSEventStreamRef streamRef);
extern FSEventStreamEventId FSEventsGetCurrentEventId(void);
extern CFUUIDRef FSEventsCopyUUIDForDevice(dev_t dev);
extern FSEventStreamEventId FSEventsGetLastEventIdForDeviceBeforeTime(dev_t dev, CFAbsoluteTime time);
extern Boolean FSEventsPurgeEventsForDeviceUpToEventId(dev_t dev, FSEventStreamEventId eventId);

extern void FSEventStreamRetain(FSEventStreamRef streamRef);
extern void FSEventStreamRelease(FSEventStreamRef streamRef);

extern void FSEventStreamScheduleWithRunLoop(FSEventStreamRef streamRef, CFRunLoopRef runLoop,
    CFStringRef runLoopMode);
extern void FSEventStreamUnscheduleFromRunLoop(FSEventStreamRef streamRef, CFRunLoopRef runLoop,
    CFStringRef runLoopMode);
extern void FSEventStreamSetDispatchQueue(FSEventStreamRef streamRef, dispatch_queue_t q);
extern void FSEventStreamInvalidate(FSEventStreamRef streamRef);

extern Boolean FSEventStreamStart(FSEventStreamRef streamRef);
extern FSEventStreamEventId FSEventStreamFlushAsync(FSEventStreamRef streamRef);
extern void FSEventStreamFlushSync(FSEventStreamRef streamRef);
extern void FSEventStreamStop(FSEventStreamRef streamRef);

extern void FSEventStreamShow(ConstFSEventStreamRef streamRef);
extern CFStringRef FSEventStreamCopyDescription(ConstFSEventStreamRef streamRef);
extern Boolean FSEventStreamSetExclusionPaths(FSEventStreamRef streamRef, CFArrayRef pathsToExclude);

#ifdef __cplusplus
}
#endif

#endif // _PD_FSEVENTS_H

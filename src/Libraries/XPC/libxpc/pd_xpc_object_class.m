#import <objc/NSObject.h>
#include <sys/types.h>
#include <mach/mach.h>
#include <xpc/launchd.h>
#include "xpc_internal.h"

@interface OS_xpc_object : NSObject
@end

@implementation OS_xpc_object

/* These objects are created by _pd_xpc_object_alloc, never by +alloc/-init. */
+ (id)alloc
{
	xpc_precondition(false, "XPC objects are not allocated through ObjC");
	return nil;
}

- (id)init
{
	xpc_precondition(false, "XPC objects are not initialised through ObjC");
	return nil;
}

- (id)retain
{
	return _pd_xpc_retain(self);
}

- (oneway void)release
{
	_pd_xpc_release(self);
}

- (NSUInteger)retainCount
{
	struct xpc_object_header *hdr = (struct xpc_object_header *)self;

	if (hdr->ref_cnt == _OS_OBJECT_GLOBAL_REFCNT) {
		return NSUIntegerMax;
	}
	return (NSUInteger)hdr->ref_cnt;
}

@end

/*
 * A connection is an xpc_object_t, so it inherits the refcounting above;
 * _pd_xpc_release already dispatches its teardown on the isa. Kept a distinct
 * class, as Darwin has it, so that isa comparison and -description can tell
 * the two apart.
 */
@interface OS_xpc_connection : OS_xpc_object
@end

@implementation OS_xpc_connection
@end

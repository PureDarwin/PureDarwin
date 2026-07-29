#ifndef _PD_SECTASK_H
#define _PD_SECTASK_H

#include <Security/SecBase.h>
#include <mach/message.h>
#include <bsm/libbsm.h>

#if __has_include(<IOKit/IOReturn.h>)
#include <IOKit/IOReturn.h>
#endif

CF_ASSUME_NONNULL_BEGIN

typedef struct __SecTask *SecTaskRef;

CF_EXPORT CFTypeID SecTaskGetTypeID(void);

CF_EXPORT SecTaskRef _Nullable SecTaskCreateWithAuditToken(CFAllocatorRef _Nullable allocator,
							   audit_token_t token);

CF_EXPORT SecTaskRef _Nullable SecTaskCreateFromSelf(CFAllocatorRef _Nullable allocator);

CF_EXPORT CFTypeRef _Nullable SecTaskCopyValueForEntitlement(SecTaskRef task,
							     CFStringRef entitlement,
							     CFErrorRef _Nullable * _Nullable error);

CF_EXPORT CFDictionaryRef _Nullable SecTaskCopyValuesForEntitlements(SecTaskRef task,
								    CFArrayRef entitlements,
								    CFErrorRef _Nullable * _Nullable error);

CF_EXPORT CFStringRef _Nullable SecTaskCopySigningIdentifier(SecTaskRef task,
							     CFErrorRef _Nullable * _Nullable error);

CF_ASSUME_NONNULL_END

#endif /* _PD_SECTASK_H */

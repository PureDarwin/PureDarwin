#ifndef SystemEntitlements_h
#define SystemEntitlements_h

/*
 This file collects entitlements defined on the platform and in use by Security.
 */

#include <CoreFoundation/CFString.h>

__BEGIN_DECLS

/* Entitlement denoting client task is an App Clip */
#define kSystemEntitlementOnDemandInstallCapable CFSTR("com.apple.developer.on-demand-install-capable")

__END_DECLS

#endif /* SystemEntitlements_h */

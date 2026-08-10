/*
 * MobileKeyBag.h - compatibility shim for Apple's MobileKeyBag framework.
 */

#ifndef _PD_MOBILEKEYBAG_H_
#define _PD_MOBILEKEYBAG_H_

#include <CoreFoundation/CoreFoundation.h>

__BEGIN_DECLS

#define kMobileKeyBagLockStatusNotificationID "com.apple.mobilekeybag.lockstatus"

/* Keys and values for the dictionary MKBUserTypeDeviceMode returns. */
extern const CFStringRef kMKBDeviceModeKey;
extern const CFStringRef kMKBDeviceModeMultiUser;
extern const CFStringRef kMKBDeviceModeSingleUser;

/*
 * Describe the device's multi-user configuration. PureDarwin is always single
 * user, so callers comparing against kMKBDeviceModeMultiUser correctly take the
 * single-user path.
 */
CFDictionaryRef MKBUserTypeDeviceMode(CFDictionaryRef options, CFErrorRef *error);

/*
 * Session ID of the foreground user. Only meaningful on multi-user iOS; here
 * there is a single session, reported as 0.
 */
uid_t MKBForegroundUserSessionID(CFErrorRef *error);

__END_DECLS

#endif /* _PD_MOBILEKEYBAG_H_ */

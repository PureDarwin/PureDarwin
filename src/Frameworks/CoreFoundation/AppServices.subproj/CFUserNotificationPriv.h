#ifndef __COREFOUNDATION_CFUSERNOTIFICATIONPRIV__
#define __COREFOUNDATION_CFUSERNOTIFICATIONPRIV__ 1

#include <CoreFoundation/CFUserNotification.h>

CF_EXTERN_C_BEGIN

/* Help book and anchor for the alert's help button, interpreted by whatever
 * displays the notification. PureDarwin has no such agent yet, so setting them
 * is inert - but it has to compile and link. */
CF_EXPORT
const CFStringRef kCFUserNotificationHelpAnchorKey;

CF_EXPORT
const CFStringRef kCFUserNotificationHelpBookKey;

CF_EXTERN_C_END

#endif /* __COREFOUNDATION_CFUSERNOTIFICATIONPRIV__ */

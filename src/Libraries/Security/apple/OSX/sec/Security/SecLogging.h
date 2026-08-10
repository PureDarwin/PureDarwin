//
//  SecLogging.h
//  sec
//
// Remote control for logging settings in securityd/secd
//


#ifndef _SECURITY_SECLOGGING_H_
#define _SECURITY_SECLOGGING_H_

#include <CoreFoundation/CoreFoundation.h>

CFArrayRef SecGetCurrentServerLoggingInfo(CFErrorRef *error);

bool SecSetLoggingInfoForXPCScope(CFPropertyListRef /* String or Dictionary of strings */ settings, CFErrorRef *error);

bool SecSetLoggingInfoForCircleScope(CFPropertyListRef /* String or Dictionary of strings */ settings, CFErrorRef *error);

#endif

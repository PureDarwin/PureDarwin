//
//  SecLogSettingsServer.h
//  sec
//
//

#ifndef _SECURITY_SECLOGSETTINGSSERVER_H_
#define _SECURITY_SECLOGSETTINGSSERVER_H_

#include <CoreFoundation/CoreFoundation.h>

__BEGIN_DECLS

CFPropertyListRef   SecCopyLogSettings_Server(CFErrorRef* error);
bool                SecSetXPCLogSettings_Server(CFTypeRef type, CFErrorRef* error);
bool                SecSetCircleLogSettings_Server(CFTypeRef type, CFErrorRef* error);

__END_DECLS

#endif

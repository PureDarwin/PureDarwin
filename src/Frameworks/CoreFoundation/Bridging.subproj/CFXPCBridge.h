/*
 * CFXPCBridge.h - conversion between CF objects and XPC objects.
 */

#ifndef __COREFOUNDATION_CFXPCBRIDGE__
#define __COREFOUNDATION_CFXPCBRIDGE__ 1

#include <CoreFoundation/CFBase.h>
#include <xpc/xpc.h>

CF_EXTERN_C_BEGIN

CF_EXPORT xpc_object_t _CFXPCCreateXPCObjectFromCFObject(CFTypeRef cf);
CF_EXPORT CFTypeRef    _CFXPCCreateCFObjectFromXPCObject(xpc_object_t xpc);

CF_EXTERN_C_END

#endif /* __COREFOUNDATION_CFXPCBRIDGE__ */

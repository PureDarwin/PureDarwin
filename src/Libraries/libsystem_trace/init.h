/*
 * init.h - declarations shared by libsystem_trace's init.c and log.c.
 *
 * The rest of libsystem_trace is Zoe Knox's original implementation vendored
 * from ravynOS (BSD-2-Clause, see log.c); that tree does not carry this header,
 * so it is reconstructed here from the definitions in init.c and the uses in
 * log.c.
 *
 * CoreFoundation is resolved lazily through dlopen/dlsym rather than linked:
 * libsystem_trace lives inside libSystem, which CoreFoundation itself depends
 * on, so a direct link would be circular.
 */

#ifndef _PUREDARWIN_LIBSYSTEM_TRACE_INIT_H_
#define _PUREDARWIN_LIBSYSTEM_TRACE_INIT_H_

#include <stdint.h>

/*
 * The handful of CoreFoundation types used here are declared locally rather
 * than by including <CoreFoundation/*.h>: CoreFoundation links against
 * libSystem, so libSystem cannot depend on its headers at build time either.
 * These match the CF definitions exactly - CFTypeRef and friends are all
 * opaque pointers, and CFIndex/CFTypeID/CFStringEncoding are the same
 * integer types CFBase.h uses.
 */
typedef const void *CFTypeRef;
typedef const struct __CFAllocator *CFAllocatorRef;
typedef const struct __CFString *CFStringRef;
typedef const struct __CFData *CFDataRef;
typedef signed long CFIndex;
typedef unsigned long CFTypeID;
typedef uint32_t CFStringEncoding;
typedef uint8_t UInt8;

#define kCFStringEncodingUTF8 0x08000100

#ifdef __cplusplus
extern "C" {
#endif

extern CFStringRef (*_CFStringCreateWithCString)(CFAllocatorRef, const char *, CFStringEncoding);
extern CFTypeID (*_CFGetTypeID)(CFTypeRef);
extern CFTypeID (*_CFDataGetTypeID)(void);
extern CFIndex (*_CFDataGetLength)(CFDataRef);
extern const uint8_t *(*_CFDataGetBytePtr)(CFDataRef);

void _libtrace_init(void);

#ifdef __cplusplus
}
#endif

#endif /* _PUREDARWIN_LIBSYSTEM_TRACE_INIT_H_ */

/*
 * Copyright (c) 2006-2007,2009-2010,2012-2014 Apple Inc. All Rights Reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

/*
 * debugging.h - non-trivial debug support
 */

/*
 * CONFIGURING DEFAULT DEBUG SCOPES
 *
 * Default debug "scope" inclusion / exclusion is configured in  com.apple.securityd.plist (iOS) and 
 * com.apple.secd.plist (OSX) in the Environmental Variable "DEBUGSCOPE".  The current value for that 
 * variable begins with a dash ("-") indicating an "exclusion list".  If you add a scope for a 
 * secnotice, etc that you don't want to always be "on" add the new string to the DEBUGSCOPE variable
 * in both plists.
 */

#ifndef _SECURITY_UTILITIES_DEBUGGING_H_
#define _SECURITY_UTILITIES_DEBUGGING_H_

#include <TargetConditionals.h>

#ifdef KERNEL
        #include <libkern/libkern.h>
        #define secalert(format, ...) printf((format), ## __VA_ARGS__)
        #define secemergency(format, ...) printf((format), ## __VA_ARGS__)
        #define seccritical(format, ...) printf((format), ## __VA_ARGS__)
        #define secerror(format, ...) printf((format), ## __VA_ARGS__)
        #define secwarning(format, ...) printf((format), ## __VA_ARGS__)
        #define secnotice(scope, format, ...) printf((format), ## __VA_ARGS__)
        #define secnoticeq(scope, format, ...) printf((format), ## __VA_ARGS__)
        #define secinfo(scope, format, ...) printf((format), ## __VA_ARGS__)
    #undef secdebug
    #if !defined(NDEBUG)
        #define secdebug(scope, format, ...) printf((format), ## __VA_ARGS__)
    #else // NDEBUG
        #define secdebug(scope, format, ...) 	/* nothing */
    #endif // NDEBUG
#else // !KERNEL

#include <CoreFoundation/CFString.h>
#include <asl.h>

__BEGIN_DECLS

#define SECLOG_LEVEL_EMERG  0
#define SECLOG_LEVEL_ALERT  1
#define SECLOG_LEVEL_CRIT   2
#define SECLOG_LEVEL_ERR    3
#define SECLOG_LEVEL_WARNING 4
#define SECLOG_LEVEL_NOTICE 5
#define SECLOG_LEVEL_INFO   6
#define SECLOG_LEVEL_DEBUG  7

#include <os/log_private.h>
extern os_log_t secLogObjForScope(const char *scope);
extern os_log_t secLogObjForCFScope(CFStringRef scope);
extern bool secLogEnabled(void);
extern void secLogDisable(void);
extern void secLogEnable(void);

CFStringRef SecLogAPICreate(bool apiIn, const char *api, CFStringRef format, ...)
    CF_FORMAT_FUNCTION(3, 4);

extern const char *api_trace;

#define sec_trace_enter_api(format...) { \
    CFStringRef info = SecLogAPICreate(true, __FUNCTION__, format, NULL); \
    secinfo(api_trace, "%@",  info); CFReleaseNull(info); \
}

#define sec_trace_return_api(rtype, body, format...) { \
    rtype _r = body(); \
    CFStringRef info = SecLogAPICreate(true, __FUNCTION__, format, _r); \
    secinfo(api_trace, "%@",  info); \
    CFReleaseNull(info); return _r; \
}

#define sec_trace_return_bool_api(body, format...) { \
    bool _r = body(); \
    CFStringRef info = SecLogAPICreate(true, __FUNCTION__, format ? format : CFSTR("return=%d"), _r); \
    secinfo(api_trace, "%@",  info); \
    CFReleaseNull(info); return _r; \
}

#define secemergency(format, ...)       os_log_error(secLogObjForScope("SecEmergency"), format, ## __VA_ARGS__)
#define secalert(format, ...)           os_log_error(secLogObjForScope("SecAlert"), format, ## __VA_ARGS__)
#define seccritical(format, ...)        os_log(secLogObjForScope("SecCritical"), format, ## __VA_ARGS__)
#define secerror(format, ...)           os_log(secLogObjForScope("SecError"), format, ## __VA_ARGS__)
#define secerrorq(format, ...)          os_log(secLogObjForScope("SecError"), format, ## __VA_ARGS__)
#define secwarning(format, ...)         os_log(secLogObjForScope("SecWarning"), format, ## __VA_ARGS__)
#define secnotice(scope, format, ...)	os_log(secLogObjForScope(scope), format, ## __VA_ARGS__)
#define secnoticeq(scope, format, ...)	os_log(secLogObjForScope(scope), format, ## __VA_ARGS__)
#define secinfo(scope, format, ...)     os_log_debug(secLogObjForScope(scope), format, ## __VA_ARGS__)

#define secinfoenabled(scope)           os_log_debug_enabled(secLogObjForScope(scope))

// secdebug is used for things that might not be privacy safe at all, so only debug builds can have these traces
#undef secdebug
#if !defined(NDEBUG)
#define secdebug(scope, format, ...)	os_log_debug(secLogObjForScope(scope), format, ## __VA_ARGS__)
#else
# define secdebug(scope,...)	/* nothing */
#endif

typedef void (^security_log_handler)(int level, CFStringRef scope, const char *function,
                                     const char *file, int line, CFStringRef message);

/* To simulate a process crash in some conditions */
void __security_simulatecrash(CFStringRef reason, uint32_t code);
void __security_stackshotreport(CFStringRef reason, uint32_t code);

/* predefined simulate crash exception codes */
#define __sec_exception_code(x) (0x53c00000+x)
/* 1 was __sec_exception_code_CorruptDb */
#define __sec_exception_code_CorruptItem            __sec_exception_code(2)
#define __sec_exception_code_OTRError               __sec_exception_code(3)
#define __sec_exception_code_DbItemDescribe         __sec_exception_code(4)
#define __sec_exception_code_TwiceCorruptDb(db)     __sec_exception_code(5|((db)<<8))
#define __sec_exception_code_AuthLoop               __sec_exception_code(6)
#define __sec_exception_code_MissingEntitlements    __sec_exception_code(7)
#define __sec_exception_code_LostInMist             __sec_exception_code(8)
#define __sec_exception_code_CKD_nil_pending_keys   __sec_exception_code(9)
#define __sec_exception_code_SQLiteBusy             __sec_exception_code(10)
#define __sec_exception_code_CorruptDb(rc)          __sec_exception_code(11|((rc)<<8))
#define __sec_exception_code_Watchdog               __sec_exception_code(12)
#define __sec_exception_code_BadStash               __sec_exception_code(13)
#define __sec_exception_code_UnexpectedState        __sec_exception_code(14)
#define __sec_exception_code_RateLimit              __sec_exception_code(15)

/* For testing only, turns off/on simulated crashes, when turning on, returns number of
   simulated crashes which were not reported since last turned off. */
int __security_simulatecrash_enable(bool enable);
bool __security_simulatecrash_enabled(void);

/* Logging control functions */

typedef enum {
    kScopeIDEnvironment = 0,
    kScopeIDDefaults = 1,
    kScopeIDConfig = 2,
    kScopeIDXPC = 3,
    kScopeIDCircle = 4,
    kScopeIDMax = 4,
} SecDebugScopeID;

void ApplyScopeListForID(CFStringRef scopeList, SecDebugScopeID whichID);
void ApplyScopeDictionaryForID(CFDictionaryRef scopeList, SecDebugScopeID whichID);
CFPropertyListRef CopyCurrentScopePlist(void);

__END_DECLS

#endif // !KERNEL

#endif /* _SECURITY_UTILITIES_DEBUGGING_H_ */
